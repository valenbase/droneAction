# RK3588 UDP JSON Protocol

The PC application listens for RK3588 commands over UDP. The default bind address is `0.0.0.0:50050`.

## Request

```json
{
  "version": 1,
  "request_id": "rk-000001",
  "command": "forward",
  "value": 1.0,
  "unit": "m/s",
  "duration_ms": 1000,
  "timestamp_ms": 1780732800000
}
```

## Test CLI

Use `tools/rk3588_command_cli.py` to simulate an RK3588 sender:

```bash
python3 tools/rk3588_command_cli.py --host 127.0.0.1 --once "前进 1米每秒 2秒"
```

For button-based testing, use the browser panel:

```bash
python3 tools/rk3588_command_panel.py --target-host 127.0.0.1
```

Print JSON without sending UDP:

```bash
python3 tools/rk3588_command_cli.py --dry-run --once "左转 20度/秒 1秒"
```

Send an exact payload:

```bash
python3 tools/rk3588_command_cli.py --json --once '{"version":1,"request_id":"rk-test","command":"hold","duration_ms":1000}'
```

## Response

```json
{
  "version": 1,
  "request_id": "rk-000001",
  "command": "forward",
  "accepted": true,
  "status": "accepted",
  "reason": "command passed safety gate",
  "execution_status": "queued",
  "execution_reason": "command queued for MAVSDK execution"
}
```

`status` is one of:

- `accepted`
- `rejected`
- `needs_confirmation`

`execution_status` appears only after the command passes the safety gate. Common values:

- `queued`: command was queued for MAVSDK execution.
- `disabled`: MAVLink execution is disabled in the selected runtime mode.
- `unavailable`: the application was built without MAVSDK.
- `not_ready`: MAVSDK is enabled but no autopilot is connected yet.

Movement commands use PX4 Offboard control inside the execution thread. If the aircraft is already in
Offboard, the application sends the requested velocity/yaw setpoint directly. If the aircraft is not in
Offboard, the application first streams the requested setpoint, requests Offboard, keeps streaming that setpoint
while waiting for mode confirmation, and aborts the movement if PX4 rejects or does not enter Offboard. PX4
warning/error status text is shown in the application status log. The UDP ACK is not a completion receipt;
mode-switch failures and command completion are reported in the application status log.

PX4 SITL mode enables MAVLink command execution by default and uses `udp://:14540`.
Real-aircraft mode disables MAVLink command execution by default; it must be enabled explicitly in Settings.

## Supported Commands

| English command | Chinese alias | Behavior |
| --- | --- | --- |
| `forward` | `前进` | Body-frame forward velocity |
| `backward` | `后退` | Body-frame backward velocity |
| `left` | `左移` | Body-frame left velocity |
| `right` | `右移` | Body-frame right velocity |
| `up` | `升高` | Vertical velocity up |
| `down` | `降低` | Vertical velocity down |
| `yaw_left` | `左转` | Yaw-rate left |
| `yaw_right` | `右转` | Yaw-rate right |
| `hold` | `悬停` | Zero movement command |
| `takeoff` | `起飞` | High-risk action |
| `land` | `降落` | High-risk action |
| `rtl` | `返航` | High-risk action |

`A计划`, `B计划`, and digit commands `0` through `9` are logged and rejected in v1 until mission definitions are supplied.

## Safety Defaults

- Movement commands require UI control authorization.
- Movement/high-risk commands require live telemetry.
- Movement values are capped by local config.
- Movement durations are capped by local config.
- `takeoff`, `land`, and `rtl` return `needs_confirmation` by default in real-aircraft mode.
- PX4 SITL mode disables high-risk confirmation by default to support local simulator testing.
