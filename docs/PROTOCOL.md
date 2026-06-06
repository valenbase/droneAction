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
  "reason": "command passed safety gate"
}
```

`status` is one of:

- `accepted`
- `rejected`
- `needs_confirmation`

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
- `takeoff`, `land`, and `rtl` return `needs_confirmation` by default.
