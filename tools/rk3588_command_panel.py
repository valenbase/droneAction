#!/usr/bin/env python3
"""
Browser command panel that sends the same UDP JSON commands as rk3588_command_cli.py.

Examples:
  python3 tools/rk3588_command_panel.py
  python3 tools/rk3588_command_panel.py --target-host 127.0.0.1 --web-port 8765
  python3 tools/rk3588_command_panel.py --bind 0.0.0.0 --target-host 127.0.0.1
"""

from __future__ import annotations

import argparse
import json
import socket
import sys
import threading
import webbrowser
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from typing import Any, Optional

from rk3588_command_cli import (
    DEFAULT_HOST,
    DEFAULT_PORT,
    ParsedCommand,
    ack_accepted,
    build_payload,
    send_movement_stream,
    send_udp,
    STREAM_COMMANDS,
)


DEFAULT_WEB_BIND = "127.0.0.1"
DEFAULT_WEB_PORT = 8765
DEFAULT_TIMEOUT = 2.0


HTML = r"""<!doctype html>
<html lang="zh-CN">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>DroneAction Control Panel</title>
  <style>
    :root {
      color-scheme: dark;
      --bg: #071018;
      --panel: #101820;
      --line: #263645;
      --line-strong: #3a5368;
      --text: #edf4fb;
      --muted: #91a4b7;
      --accent: #23c0a4;
      --accent-2: #769cff;
      --warn: #ffbe5c;
      --danger: #ff6b6b;
      --button: #13202b;
      --button-hover: #182b39;
    }

    * { box-sizing: border-box; }

    body {
      margin: 0;
      min-height: 100vh;
      background: var(--bg);
      color: var(--text);
      font-family: Inter, Arial, "Microsoft YaHei", sans-serif;
    }

    main {
      width: min(1180px, calc(100vw - 32px));
      margin: 0 auto;
      padding: 18px 0 24px;
    }

    header {
      display: grid;
      grid-template-columns: 1fr auto;
      gap: 16px;
      align-items: center;
      margin-bottom: 14px;
    }

    h1 {
      margin: 0;
      font-size: 24px;
      font-weight: 800;
      letter-spacing: 0;
    }

    .status {
      min-width: 180px;
      min-height: 34px;
      border: 1px solid var(--line);
      border-radius: 6px;
      display: flex;
      align-items: center;
      justify-content: center;
      padding: 6px 10px;
      color: var(--muted);
      background: #0c1721;
      font-weight: 700;
    }

    .status.ok {
      color: #e8fff9;
      background: #0d302c;
      border-color: var(--accent);
    }

    .status.fail {
      color: #ffe9e9;
      background: #331616;
      border-color: var(--danger);
    }

    .layout {
      display: grid;
      grid-template-columns: minmax(320px, 420px) 1fr;
      gap: 14px;
    }

    section {
      background: var(--panel);
      border: 1px solid var(--line);
      border-radius: 8px;
      padding: 14px;
    }

    section + section { margin-top: 14px; }

    h2 {
      margin: 0 0 12px;
      font-size: 15px;
      color: #b9d4ee;
      font-weight: 800;
      letter-spacing: 0;
    }

    .form-grid {
      display: grid;
      grid-template-columns: 1fr 120px;
      gap: 10px;
    }

    label {
      display: grid;
      gap: 5px;
      color: var(--muted);
      font-size: 12px;
      font-weight: 700;
    }

    input {
      width: 100%;
      height: 36px;
      background: #0c1721;
      color: var(--text);
      border: 1px solid #30475d;
      border-radius: 6px;
      padding: 7px 9px;
      font: inherit;
      font-size: 14px;
    }

    input:focus {
      outline: 2px solid rgba(35, 192, 164, 0.45);
      border-color: var(--accent);
    }

    .quick-grid {
      display: grid;
      grid-template-columns: repeat(3, minmax(86px, 1fr));
      gap: 10px;
    }

    .actions {
      display: grid;
      grid-template-columns: repeat(3, minmax(94px, 1fr));
      gap: 10px;
    }

    button {
      min-height: 48px;
      border: 1px solid var(--line-strong);
      border-radius: 8px;
      background: var(--button);
      color: var(--text);
      font: inherit;
      font-size: 15px;
      font-weight: 800;
      cursor: pointer;
    }

    button:hover { background: var(--button-hover); border-color: #5e7c96; }
    button:active { transform: translateY(1px); }
    button:disabled { opacity: 0.55; cursor: not-allowed; transform: none; }

    button.primary { border-color: var(--accent); background: #0d302c; }
    button.primary:hover { background: #123d37; }
    button.warn { border-color: var(--warn); background: #332313; }
    button.danger { border-color: var(--danger); background: #321616; }
    button.neutral { color: #dce9f7; }

    .dpad {
      display: grid;
      grid-template-columns: repeat(3, 1fr);
      grid-template-rows: repeat(3, 66px);
      gap: 10px;
    }

    .dpad button[data-command="forward"] { grid-column: 2; grid-row: 1; }
    .dpad button[data-command="left"] { grid-column: 1; grid-row: 2; }
    .dpad button[data-command="hold"] { grid-column: 2; grid-row: 2; }
    .dpad button[data-command="right"] { grid-column: 3; grid-row: 2; }
    .dpad button[data-command="backward"] { grid-column: 2; grid-row: 3; }

    .log {
      min-height: 420px;
      max-height: 62vh;
      overflow: auto;
      background: #05080c;
      border: 1px solid #263645;
      border-radius: 8px;
      padding: 12px;
      font-family: ui-monospace, SFMono-Regular, Consolas, "Liberation Mono", monospace;
      font-size: 13px;
      line-height: 1.45;
      white-space: pre-wrap;
      overflow-wrap: anywhere;
    }

    .hint {
      margin: 10px 0 0;
      color: var(--muted);
      font-size: 12px;
      line-height: 1.5;
    }

    @media (max-width: 860px) {
      main { width: min(100vw - 20px, 640px); padding-top: 12px; }
      header { grid-template-columns: 1fr; }
      .layout { grid-template-columns: 1fr; }
      .form-grid { grid-template-columns: 1fr 1fr; }
      .quick-grid, .actions { grid-template-columns: repeat(2, minmax(0, 1fr)); }
    }
  </style>
</head>
<body>
<main>
  <header>
    <h1>DroneAction Control Panel</h1>
    <div id="status" class="status">Ready</div>
  </header>

  <div class="layout">
    <div>
      <section>
        <h2>Connection</h2>
        <div class="form-grid">
          <label>Target Host
            <input id="targetHost" value="{{target_host}}" autocomplete="off">
          </label>
          <label>Port
            <input id="targetPort" type="number" min="1" max="65535" value="{{target_port}}">
          </label>
          <label>ACK Timeout
            <input id="timeout" type="number" min="0.1" max="20" step="0.1" value="{{timeout}}">
          </label>
          <label>Duration
            <input id="duration" type="number" min="100" max="300000" step="100" value="2000">
          </label>
          <label>Move Speed
            <input id="speed" type="number" min="0.1" max="30" step="0.1" value="1.0">
          </label>
          <label>Vertical Speed
            <input id="verticalSpeed" type="number" min="0.1" max="10" step="0.1" value="0.5">
          </label>
          <label>Yaw Rate
            <input id="yawRate" type="number" min="1" max="180" step="1" value="20">
          </label>
          <label>Takeoff Alt
            <input id="takeoffAlt" type="number" min="0" max="120" step="0.5" value="2.5">
          </label>
        </div>
        <p class="hint">Movement buttons use Duration. Takeoff/Land/Return may require confirmation in the main app.</p>
      </section>

      <section>
        <h2>Quick Actions</h2>
        <div class="actions">
          <button class="primary" data-command="takeoff" data-kind="takeoff">起飞</button>
          <button class="warn" data-command="land" data-kind="simple">降落</button>
          <button class="warn" data-command="rtl" data-kind="simple">返航</button>
        </div>
      </section>

      <section>
        <h2>Movement</h2>
        <div class="dpad">
          <button data-command="forward" data-kind="move">前进</button>
          <button data-command="left" data-kind="move">左移</button>
          <button class="danger" data-command="hold" data-kind="simple">悬停</button>
          <button data-command="right" data-kind="move">右移</button>
          <button data-command="backward" data-kind="move">后退</button>
        </div>
      </section>

      <section>
        <h2>Vertical / Yaw</h2>
        <div class="quick-grid">
          <button data-command="up" data-kind="vertical">升高</button>
          <button data-command="down" data-kind="vertical">降低</button>
          <button class="neutral" data-command="yaw_left" data-kind="yaw">左转</button>
          <button class="neutral" data-command="yaw_right" data-kind="yaw">右转</button>
        </div>
      </section>
    </div>

    <section>
      <h2>Request / ACK</h2>
      <div id="log" class="log">Click a command button to send.</div>
    </section>
  </div>
</main>

<script>
const statusEl = document.getElementById("status");
const logEl = document.getElementById("log");

function numberValue(id) {
  const value = Number(document.getElementById(id).value);
  if (!Number.isFinite(value)) throw new Error(`${id} is invalid`);
  return value;
}

function commonOptions() {
  return {
    host: document.getElementById("targetHost").value.trim(),
    port: numberValue("targetPort"),
    timeout: numberValue("timeout"),
  };
}

function commandPayload(button) {
  const command = button.dataset.command;
  const kind = button.dataset.kind;
  const payload = { command, ...commonOptions() };

  if (kind === "move") {
    payload.value = numberValue("speed");
    payload.unit = "m/s";
    payload.duration_ms = numberValue("duration");
  } else if (kind === "vertical") {
    payload.value = numberValue("verticalSpeed");
    payload.unit = "m/s";
    payload.duration_ms = numberValue("duration");
  } else if (kind === "yaw") {
    payload.value = numberValue("yawRate");
    payload.unit = "deg/s";
    payload.duration_ms = numberValue("duration");
  } else if (kind === "takeoff") {
    payload.value = numberValue("takeoffAlt");
    payload.unit = "m";
  }
  return payload;
}

let pendingRequests = 0;
function setBusy(busy) {
  document.querySelectorAll("button").forEach((button) => {
    button.disabled = busy && button.dataset.command !== "hold";
  });
  statusEl.textContent = busy ? "Sending" : "Ready";
  statusEl.className = "status";
}

function showResult(data, ok) {
  statusEl.textContent = ok ? "Accepted" : "Rejected";
  statusEl.className = ok ? "status ok" : "status fail";
  logEl.textContent = JSON.stringify(data, null, 2);
}

async function sendCommand(button) {
  let payload;
  try {
    payload = commandPayload(button);
  } catch (error) {
    showResult({ error: String(error) }, false);
    return;
  }

  pendingRequests += 1;
  setBusy(true);
  try {
    const response = await fetch("/api/send", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(payload),
    });
    const data = await response.json();
    showResult(data, Boolean(data.ok));
  } catch (error) {
    showResult({ request: payload, error: String(error) }, false);
  } finally {
    pendingRequests -= 1;
    document.querySelectorAll("button").forEach((button) => {
      button.disabled = pendingRequests > 0 && button.dataset.command !== "hold";
    });
  }
}

document.querySelectorAll("button[data-command]").forEach((button) => {
  button.addEventListener("click", () => sendCommand(button));
});
</script>
</body>
</html>
"""


def as_float(value: Any, fallback: Optional[float] = None) -> Optional[float]:
    if value is None or value == "":
        return fallback
    return float(value)


def as_int(value: Any, fallback: Optional[int] = None) -> Optional[int]:
    if value is None or value == "":
        return fallback
    return int(float(value))


def unit_for_command(command: str, requested_unit: str) -> str:
    if requested_unit:
        return requested_unit
    if command in {"yaw_left", "yaw_right", "turn_left", "turn_right"}:
        return "deg/s"
    if command == "takeoff":
        return "m"
    if command in {"forward", "backward", "left", "right", "up", "down"}:
        return "m/s"
    return ""


class PanelState:
    def __init__(self, target_host: str, target_port: int, timeout: float) -> None:
        self.target_host = target_host
        self.target_port = target_port
        self.timeout = timeout
        self.command_lock = threading.Lock()
        self.stream_stop = threading.Event()

    def send(self, payload: dict[str, Any], host: str, port: int, timeout: float) -> tuple:
        self.stream_stop.set()
        with self.command_lock:
            if payload["command"].lower() in STREAM_COMMANDS:
                self.stream_stop = threading.Event()
                return send_movement_stream(payload, host, port, timeout, True,
                                            stop_event=self.stream_stop)
            return send_udp(payload, host, port, timeout, True), 0, None


def make_handler(state: PanelState) -> type[BaseHTTPRequestHandler]:
    class CommandPanelHandler(BaseHTTPRequestHandler):
        server_version = "DroneActionCommandPanel/1.0"

        def log_message(self, format: str, *args: Any) -> None:
            print(f"{self.address_string()} - {format % args}", file=sys.stderr)

        def do_GET(self) -> None:
            if self.path in {"/", "/index.html"}:
                html = (HTML
                        .replace("{{target_host}}", state.target_host)
                        .replace("{{target_port}}", str(state.target_port))
                        .replace("{{timeout}}", f"{state.timeout:g}"))
                self.write_bytes(HTTPStatus.OK, html.encode("utf-8"), "text/html; charset=utf-8")
                return
            if self.path == "/health":
                self.write_json(HTTPStatus.OK, {"ok": True})
                return
            self.write_json(HTTPStatus.NOT_FOUND, {"ok": False, "error": "not found"})

        def do_POST(self) -> None:
            if self.path != "/api/send":
                self.write_json(HTTPStatus.NOT_FOUND, {"ok": False, "error": "not found"})
                return

            try:
                payload = self.read_json()
                response = self.send_command(payload)
                status = HTTPStatus.OK if response.get("ok") else HTTPStatus.BAD_REQUEST
                self.write_json(status, response)
            except (ValueError, TypeError, json.JSONDecodeError) as exc:
                self.write_json(HTTPStatus.BAD_REQUEST, {"ok": False, "error": str(exc)})

        def read_json(self) -> dict[str, Any]:
            length = int(self.headers.get("Content-Length", "0"))
            if length <= 0:
                raise ValueError("missing JSON body")
            data = self.rfile.read(length)
            value = json.loads(data.decode("utf-8"))
            if not isinstance(value, dict):
                raise ValueError("JSON body must be an object")
            return value

        def send_command(self, request: dict[str, Any]) -> dict[str, Any]:
            command = str(request.get("command", "")).strip()
            if not command:
                raise ValueError("command is required")

            host = str(request.get("host") or state.target_host).strip()
            if not host:
                raise ValueError("host is required")
            port = as_int(request.get("port"), state.target_port)
            timeout = as_float(request.get("timeout"), state.timeout)
            if port is None or port < 1 or port > 65535:
                raise ValueError("port must be between 1 and 65535")
            if timeout is None or timeout <= 0:
                raise ValueError("timeout must be greater than 0")

            value = as_float(request.get("value"), None)
            duration_ms = as_int(request.get("duration_ms"), None)
            unit = unit_for_command(command, str(request.get("unit", "")).strip())
            parsed = ParsedCommand(
                command=command,
                raw=command,
                value=value,
                unit=unit,
                duration_ms=duration_ms or 0,
            )
            udp_payload = build_payload(parsed, None, None, None, None)

            stream_packets = 0
            hold_ack = None
            try:
                ack, stream_packets, hold_ack = state.send(udp_payload, host, port, timeout)
            except socket.timeout:
                return {
                    "ok": False,
                    "request": udp_payload,
                    "error": f"ACK timeout after {timeout:g}s",
                }
            except OSError as exc:
                return {
                    "ok": False,
                    "request": udp_payload,
                    "error": f"UDP send failed: {exc}",
                }
            except json.JSONDecodeError as exc:
                return {
                    "ok": False,
                    "request": udp_payload,
                    "error": f"invalid ACK JSON: {exc}",
                }

            accepted = ack_accepted(ack) and (hold_ack is None or ack_accepted(hold_ack))
            return {
                "ok": accepted,
                "request": udp_payload,
                "ack": ack,
                "stream_packets": stream_packets,
                "hold_ack": hold_ack,
            }

        def write_json(self, status: HTTPStatus, payload: dict[str, Any]) -> None:
            data = json.dumps(payload, ensure_ascii=False, indent=2).encode("utf-8")
            self.write_bytes(status, data, "application/json; charset=utf-8")

        def write_bytes(self, status: HTTPStatus, data: bytes, content_type: str) -> None:
            self.send_response(status)
            self.send_header("Content-Type", content_type)
            self.send_header("Content-Length", str(len(data)))
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            self.wfile.write(data)

    return CommandPanelHandler


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Start a browser control panel for DroneAction UDP commands.")
    parser.add_argument("--bind", default=DEFAULT_WEB_BIND, help=f"Panel bind address, default {DEFAULT_WEB_BIND}")
    parser.add_argument("--web-port", type=int, default=DEFAULT_WEB_PORT, help=f"Panel HTTP port, default {DEFAULT_WEB_PORT}")
    parser.add_argument("--target-host", default=DEFAULT_HOST, help=f"DroneAction host, default {DEFAULT_HOST}")
    parser.add_argument("--target-port", type=int, default=DEFAULT_PORT, help=f"DroneAction command port, default {DEFAULT_PORT}")
    parser.add_argument("--timeout", type=float, default=DEFAULT_TIMEOUT, help=f"ACK timeout seconds, default {DEFAULT_TIMEOUT:g}")
    parser.add_argument("--no-browser", action="store_true", help="Do not open the browser automatically")
    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    state = PanelState(args.target_host, args.target_port, args.timeout)
    server = ThreadingHTTPServer((args.bind, args.web_port), make_handler(state))
    host, port = server.server_address[:2]
    display_host = "127.0.0.1" if host in {"0.0.0.0", "::"} else host
    url = f"http://{display_host}:{port}/"
    print(f"DroneAction command panel: {url}")
    print(f"Sending commands to udp://{state.target_host}:{state.target_port}")
    print("Press Ctrl+C to stop.")

    if not args.no_browser:
        threading.Timer(0.4, lambda: webbrowser.open(url)).start()

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print()
    finally:
        server.server_close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
