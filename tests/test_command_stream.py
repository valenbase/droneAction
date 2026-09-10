import json
from pathlib import Path
import socket
import sys
import threading
import time
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from rk3588_command_cli import send_movement_stream
from rk3588_command_panel import PanelState


class Peer:
    def __init__(self, delay=0.0):
        self.delay = delay
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.socket.bind(("127.0.0.1", 0))
        self.socket.settimeout(0.01)
        self.port = self.socket.getsockname()[1]
        self.packets = []
        self.stop = threading.Event()
        self.thread = threading.Thread(target=self.run)

    def __enter__(self):
        self.thread.start()
        return self

    def __exit__(self, *_):
        self.stop.set()
        self.thread.join(2)
        self.socket.close()

    def run(self):
        pending = []
        while not self.stop.is_set():
            try:
                data, address = self.socket.recvfrom(4096)
                packet = json.loads(data)
                now = time.monotonic()
                self.packets.append((now, packet))
                pending.append((now + self.delay, address, packet))
            except socket.timeout:
                pass
            now = time.monotonic()
            for due, address, packet in pending:
                if due <= now:
                    ack = dict(packet, accepted=True, status="accepted")
                    self.socket.sendto(json.dumps(ack).encode(), address)
            pending = [entry for entry in pending if entry[0] > now]


class CommandStreamTests(unittest.TestCase):
    def test_delayed_ack_does_not_delay_targets(self):
        with Peer(0.25) as peer:
            ack, count, hold = send_movement_stream(
                {"command": "forward", "value": 1, "duration_ms": 500},
                "127.0.0.1", peer.port, 1, True, 20)
            packets = [entry for entry in peer.packets if entry[1]["command"] == "forward"]
            self.assertGreaterEqual(count, 9)
            self.assertTrue(ack["accepted"] and hold["accepted"])
            self.assertEqual([p["sequence"] for _, p in packets], list(range(1, count + 1)))
            self.assertLess(max(b[0] - a[0] for a, b in zip(packets, packets[1:])), 0.15)

    def test_panel_hold_interrupts_long_running_movement(self):
        with Peer() as peer:
            panel = PanelState("127.0.0.1", peer.port, 1)
            errors = []

            def move():
                try:
                    panel.send({"command": "forward", "value": 1, "duration_ms": 30000},
                               "127.0.0.1", peer.port, 1)
                except Exception as error:
                    errors.append(error)

            worker = threading.Thread(target=move)
            worker.start()
            try:
                deadline = time.monotonic() + 2
                while not peer.packets and time.monotonic() < deadline:
                    time.sleep(0.01)
                self.assertTrue(peer.packets)
                start = time.monotonic()
                panel.send({"command": "hold"}, "127.0.0.1", peer.port, 1)
                worker.join(2)
                self.assertFalse(worker.is_alive())
                self.assertLess(time.monotonic() - start, 1)
                self.assertFalse(errors)
                first_hold = next(i for i, (_, p) in enumerate(peer.packets) if p["command"] == "hold")
                self.assertTrue(all(p["command"] == "hold" for _, p in peer.packets[first_hold:]))
            finally:
                panel.stream_stop.set()
                worker.join(2)


if __name__ == "__main__":
    unittest.main()
