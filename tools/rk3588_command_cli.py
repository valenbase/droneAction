#!/usr/bin/env python3
"""
Test CLI that simulates an RK3588 sending UDP JSON commands to DroneAction.

Examples:
  python3 tools/rk3588_command_cli.py --host 127.0.0.1 --once "forward 1m/s 2s"
  python3 tools/rk3588_command_cli.py --host 192.168.0.12 --once "前进 1米每秒 2秒"
  python3 tools/rk3588_command_cli.py --dry-run --once "左转 20度/秒 1秒"
"""

from __future__ import annotations

import argparse
import json
import re
import select
import socket
import sys
import threading
import time
import uuid
from dataclasses import dataclass
from typing import Optional


DEFAULT_HOST = "127.0.0.1"
DEFAULT_PORT = 50050
DEFAULT_DURATION_MS = 1000
DEFAULT_STREAM_HZ = 10.0
STREAM_COMMANDS = {
    "forward", "backward", "back", "left", "right", "up", "down",
    "yaw_left", "yaw_right", "turn_left", "turn_right",
}


class CommandParseError(ValueError):
    pass


@dataclass
class ParsedCommand:
    command: str
    raw: str
    value: Optional[float] = None
    unit: str = ""
    duration_ms: int = DEFAULT_DURATION_MS


def compact_text(text: str) -> str:
    return re.sub(r"\s+", "", text.strip().lower())


def first_number(text: str) -> Optional[float]:
    match = re.search(r"[-+]?\d+(?:\.\d+)?", text)
    return float(match.group(0)) if match else None


def duration_ms_from_text(text: str) -> Optional[int]:
    match = re.search(r"([-+]?\d+(?:\.\d+)?)\s*(?:秒钟|秒|seconds|second|secs|sec|s)", text, re.I)
    if not match:
        return None
    return max(1, int(float(match.group(1)) * 1000))


def speed_from_text(text: str) -> Optional[float]:
    patterns = [
        r"([-+]?\d+(?:\.\d+)?)\s*(?:m/s|米/秒|米每秒|米每秒钟|mps)",
        r"(?:速度|速率)\s*([-+]?\d+(?:\.\d+)?)",
        r"以\s*([-+]?\d+(?:\.\d+)?)\s*(?:米|m)?\s*(?:每秒|/秒|/s)",
    ]
    for pattern in patterns:
        match = re.search(pattern, text, re.I)
        if match:
            return float(match.group(1))
    return None


def yaw_rate_from_text(text: str) -> Optional[float]:
    match = re.search(r"([-+]?\d+(?:\.\d+)?)\s*(?:度/秒|度每秒|deg/s|dps)", text, re.I)
    return float(match.group(1)) if match else None


def parse_command(text: str, default_speed: float, default_yaw_rate: float) -> ParsedCommand:
    raw = text.strip()
    compact = compact_text(raw)
    if not compact:
        raise CommandParseError("empty command")

    direct_tokens = {
        "forward",
        "backward",
        "back",
        "left",
        "right",
        "up",
        "down",
        "yaw_left",
        "turn_left",
        "yaw_right",
        "turn_right",
        "hold",
        "hover",
        "takeoff",
        "land",
        "rtl",
        "return",
        "plan_a",
        "plan_b",
    }
    if compact in direct_tokens or (len(compact) == 1 and compact.isdigit()):
        return ParsedCommand(command=compact, raw=raw, duration_ms=duration_ms_from_text(raw) or DEFAULT_DURATION_MS)

    if any(key in compact for key in ("帮助", "help", "?")):
        return ParsedCommand(command="help", raw=raw)

    if any(key in compact for key in ("降落", "着陆", "land")):
        return ParsedCommand(command="land", raw=raw)

    if any(key in compact for key in ("返航", "返回", "rtl", "return")):
        return ParsedCommand(command="rtl", raw=raw)

    if any(key in compact for key in ("悬停", "停止", "刹车", "hover", "hold", "stop")):
        return ParsedCommand(command="hold", raw=raw)

    if any(key in compact for key in ("起飞", "takeoff")):
        return ParsedCommand(
            command="takeoff",
            raw=raw,
            value=first_number(compact),
            unit="m",
            duration_ms=duration_ms_from_text(raw) or DEFAULT_DURATION_MS,
        )

    duration_ms = duration_ms_from_text(raw) or DEFAULT_DURATION_MS
    speed = speed_from_text(raw)

    if any(key in compact for key in ("左转", "向左转", "yawleft", "turnleft")):
        return ParsedCommand(
            command="yaw_left",
            raw=raw,
            value=yaw_rate_from_text(raw) or default_yaw_rate,
            unit="deg/s",
            duration_ms=duration_ms,
        )

    if any(key in compact for key in ("右转", "向右转", "yawright", "turnright")):
        return ParsedCommand(
            command="yaw_right",
            raw=raw,
            value=yaw_rate_from_text(raw) or default_yaw_rate,
            unit="deg/s",
            duration_ms=duration_ms,
        )

    movement_map = [
        (("前进", "向前", "forward"), "forward"),
        (("后退", "向后", "backward", "back"), "backward"),
        (("左移", "向左", "left"), "left"),
        (("右移", "向右", "right"), "right"),
        (("升高", "上升", "向上", "up"), "up"),
        (("降低", "下降", "向下", "down"), "down"),
    ]
    for keys, command in movement_map:
        if any(key in compact for key in keys):
            return ParsedCommand(
                command=command,
                raw=raw,
                value=speed if speed is not None else default_speed,
                unit="m/s",
                duration_ms=duration_ms,
            )

    raise CommandParseError(f"unrecognized command: {raw}")


def build_payload(parsed: ParsedCommand, request_id: Optional[str], force_value: Optional[float],
                  force_unit: Optional[str], force_duration_ms: Optional[int]) -> dict:
    payload = {
        "version": 1,
        "request_id": request_id or f"rk-{uuid.uuid4().hex[:8]}",
        "command": parsed.command,
        "timestamp_ms": int(time.time() * 1000),
    }
    value = force_value if force_value is not None else parsed.value
    unit = force_unit if force_unit is not None else parsed.unit
    duration_ms = force_duration_ms if force_duration_ms is not None else parsed.duration_ms
    if value is not None:
        payload["value"] = value
    if unit:
        payload["unit"] = unit
    if duration_ms:
        payload["duration_ms"] = duration_ms
    return payload


def send_udp(payload: dict, host: str, port: int, timeout: float, wait_ack: bool) -> Optional[dict]:
    data = json.dumps(payload, ensure_ascii=False, separators=(",", ":")).encode("utf-8")
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.settimeout(timeout)
        sock.sendto(data, (host, port))
        if not wait_ack:
            return None
        response, _ = sock.recvfrom(4096)
    return json.loads(response.decode("utf-8"))


def ack_accepted(ack: Optional[dict]) -> bool:
    return bool(ack and (ack.get("accepted") or ack.get("status") == "needs_confirmation"))


def send_movement_stream(payload: dict, host: str, port: int, timeout: float,
                         wait_ack: bool, stream_hz: float = DEFAULT_STREAM_HZ,
                         stop_event: Optional[threading.Event] = None) -> tuple[Optional[dict], int, Optional[dict]]:
    if stream_hz <= 0:
        raise ValueError("stream_hz must be greater than 0")

    duration_ms = max(100, int(payload.get("duration_ms") or DEFAULT_DURATION_MS))
    interval = 1.0 / stream_hz
    deadline = time.monotonic() + duration_ms / 1000.0
    base_request_id = str(payload.get("request_id") or f"rk-{uuid.uuid4().hex[:8]}")
    session_id = f"test-{uuid.uuid4().hex[:8]}"
    first_ack: Optional[dict] = None
    packet_count = 0
    next_send = time.monotonic()

    def receive_available(sock: socket.socket, wait_seconds: float) -> list[dict]:
        if wait_seconds < 0:
            wait_seconds = 0
        readable, _, _ = select.select([sock], [], [], wait_seconds)
        if not readable:
            return []

        acknowledgements = []
        while True:
            try:
                response, _ = sock.recvfrom(4096)
            except BlockingIOError:
                break
            acknowledgements.append(json.loads(response.decode("utf-8")))
        return acknowledgements

    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.setblocking(False)
        while (packet_count == 0 or time.monotonic() < deadline) and not (stop_event and stop_event.is_set()):
            now = time.monotonic()
            if now < next_send:
                wait_seconds = min(next_send, deadline) - now
                if wait_ack:
                    for ack in receive_available(sock, wait_seconds):
                        if first_ack is None:
                            first_ack = ack
                        if not ack_accepted(ack):
                            return ack, packet_count, None
                elif wait_seconds > 0:
                    time.sleep(wait_seconds)
                continue

            packet_count += 1
            update = dict(payload)
            update["request_id"] = f"{base_request_id}-{packet_count}"
            update["session_id"] = session_id
            update["sequence"] = packet_count
            update["timestamp_ms"] = int(time.time() * 1000)
            data = json.dumps(update, ensure_ascii=False, separators=(",", ":")).encode("utf-8")
            sock.sendto(data, (host, port))
            next_send = max(next_send + interval, time.monotonic())

        if wait_ack:
            for ack in receive_available(sock, 0):
                if first_ack is None:
                    first_ack = ack
                if not ack_accepted(ack):
                    return ack, packet_count, None

        hold = {
            "version": 1,
            "request_id": f"{base_request_id}-hold",
            "session_id": session_id,
            "sequence": packet_count + 1,
            "command": "hold",
            "timestamp_ms": int(time.time() * 1000),
        }
        data = json.dumps(hold, ensure_ascii=False, separators=(",", ":")).encode("utf-8")
        sock.sendto(data, (host, port))
        hold_ack = None
        if wait_ack:
            ack_deadline = time.monotonic() + timeout
            while hold_ack is None and time.monotonic() < ack_deadline:
                for ack in receive_available(sock, ack_deadline - time.monotonic()):
                    if first_ack is None:
                        first_ack = ack
                    if ack.get("request_id") == hold["request_id"]:
                        hold_ack = ack
                        break
            if hold_ack is None:
                raise socket.timeout
    return first_ack, packet_count, hold_ack


def print_payload(label: str, payload: dict) -> None:
    print(f"{label}:")
    print(json.dumps(payload, ensure_ascii=False, indent=2))


def run_once(args: argparse.Namespace, text: str) -> int:
    if args.raw_json:
        payload = json.loads(text)
    else:
        parsed = parse_command(text, args.default_speed, args.default_yaw_rate)
        if parsed.command == "help":
            print_help()
            return 0
        payload = build_payload(parsed, args.request_id, args.value, args.unit, args.duration_ms)

    print_payload("request", payload)
    if args.dry_run:
        return 0

    hold_ack = None
    try:
        command = str(payload.get("command", "")).strip().lower()
        if command in STREAM_COMMANDS and not args.single_datagram:
            ack, packet_count, hold_ack = send_movement_stream(
                payload,
                args.host,
                args.port,
                args.timeout,
                not args.no_ack,
                args.stream_hz,
            )
            print(f"stream: {packet_count} movement updates at {args.stream_hz:g} Hz")
            if hold_ack is not None:
                print_payload("hold_ack", hold_ack)
        else:
            ack = send_udp(payload, args.host, args.port, args.timeout, not args.no_ack)
    except socket.timeout:
        print(f"ACK timeout after {args.timeout:g}s", file=sys.stderr)
        return 1
    except OSError as exc:
        print(f"UDP send failed: {exc}", file=sys.stderr)
        return 1
    except json.JSONDecodeError as exc:
        print(f"invalid ACK JSON: {exc}", file=sys.stderr)
        return 1

    if ack is not None:
        print_payload("ack", ack)
        stream_safely_stopped = hold_ack is None or ack_accepted(hold_ack)
        return 0 if ack_accepted(ack) and stream_safely_stopped else 2
    return 0


def print_help() -> None:
    print(
        """
Commands:
  前进 / forward              body-frame forward velocity
  后退 / backward             body-frame backward velocity
  左移 / left                 body-frame left velocity
  右移 / right                body-frame right velocity
  升高 / up                   vertical up velocity
  降低 / down                 vertical down velocity
  左转 / yaw_left             yaw left rate
  右转 / yaw_right            yaw right rate
  悬停 / hold                 zero velocity while remaining in external control
  起飞 / takeoff              high-risk action, normally needs UI confirmation
  降落 / land                 high-risk action, normally needs UI confirmation
  返航 / rtl                  high-risk action, normally needs UI confirmation

Examples:
  前进 1米每秒 2秒
  左转 20度/秒 1秒
  升高 0.5m/s 3s
  hold
""".strip()
    )


def interactive_loop(args: argparse.Namespace) -> int:
    print(f"DroneAction RK3588 test CLI -> udp://{args.host}:{args.port}")
    print("Type help for commands, quit to exit.")
    while True:
        try:
            text = input("> ").strip()
        except EOFError:
            print()
            return 0
        if not text:
            continue
        if compact_text(text) in {"退出", "quit", "exit", "q"}:
            return 0
        code = run_once(args, text)
        if code == 1:
            return code


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Simulate RK3588 UDP commands for DroneAction.")
    parser.add_argument("--host", default=DEFAULT_HOST, help=f"PC address running DroneAction, default {DEFAULT_HOST}")
    parser.add_argument("--port", type=int, default=DEFAULT_PORT, help=f"DroneAction UDP port, default {DEFAULT_PORT}")
    parser.add_argument("--timeout", type=float, default=2.0, help="ACK timeout in seconds")
    parser.add_argument("--once", help="Send one natural-language or token command and exit")
    parser.add_argument("--json", dest="raw_json", action="store_true", help="Treat --once text as exact JSON payload")
    parser.add_argument("--dry-run", action="store_true", help="Print request JSON without sending UDP")
    parser.add_argument("--no-ack", action="store_true", help="Do not wait for DroneAction ACK")
    parser.add_argument("--request-id", help="Override request_id")
    parser.add_argument("--value", type=float, help="Override value")
    parser.add_argument("--unit", help="Override unit")
    parser.add_argument("--duration-ms", type=int, help="Override duration_ms")
    parser.add_argument("--stream-hz", type=float, default=DEFAULT_STREAM_HZ,
                        help=f"Movement update rate, default {DEFAULT_STREAM_HZ:g} Hz")
    parser.add_argument("--single-datagram", action="store_true",
                        help="Send movement as one legacy datagram without periodic updates")
    parser.add_argument("--default-speed", type=float, default=0.8, help="Default movement speed in m/s")
    parser.add_argument("--default-yaw-rate", type=float, default=20.0, help="Default yaw rate in deg/s")
    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    if args.once:
        try:
            return run_once(args, args.once)
        except (CommandParseError, json.JSONDecodeError) as exc:
            print(exc, file=sys.stderr)
            return 2
    return interactive_loop(args)


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
