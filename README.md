# DroneAction

DroneAction is a Linux AppImage for monitoring and controlling multirotor aircraft through a Skydroid G12 data/video link. It supports both PX4 and ArduPilot/ArduCopter and selects the appropriate control backend after receiving the aircraft heartbeat.

The first milestone provides:

- A Qt6/C++ desktop monitoring console.
- Startup selection between simulation and real-aircraft operation.
- OpenStreetMap-based GPS map view with an aircraft heading marker and local tile cache.
- Interface endpoints are edited from the top settings button and stored under the app config directory.
- Default G12 shared-network endpoints from the local G12 manual:
  - MAVLink telemetry/control: `tcp://192.168.43.1:5760`
  - RTSP video: `rtsp://<user>:<password>@192.168.144.123/stream=0`
- RK3588 UDP JSON command intake on `0.0.0.0:50050`.
- Low-latency RTSP video using a 50 ms UDP jitter buffer, late-frame dropping, and automatic reconnect.
- Simulation defaults use `udp://:14540`; ArduCopter SITL can be selected in Settings with `tcp://127.0.0.1:5760`.
- Command execution is enabled by default for simulation and disabled by default for a new real-aircraft profile.
- ArduCopter movement uses a closed-loop Guided workflow: request mode, confirm mode from heartbeat, then stream body-frame velocity targets. Takeoff confirms Guided and motor arm before sending the takeoff target.
- PX4 movement uses one persistent control session. Incoming UDP movement packets update the target without restarting the flight mode. When a command finishes or updates stop, the session stays in guided control and streams zero velocity. It leaves that mode when the operator disables movement authorization, or when the aircraft leaves the mode itself.
- A safety gate for movement/high-risk commands.
- GitHub Actions packaging for `x86_64` AppImage releases.

## Local Build

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

Local builds require Qt6, GStreamer 1.18 or newer (core, app, video, RTSP and codec plugins),
CMake, Ninja, pkg-config, and a C++20 compiler. On Ubuntu 22.04, install the video build packages with:

```bash
sudo apt install libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev \
  gstreamer1.0-plugins-base gstreamer1.0-plugins-good \
  gstreamer1.0-plugins-bad gstreamer1.0-plugins-ugly gstreamer1.0-libav
```

MAVSDK can be enabled with:

```bash
cmake -S . -B build -G Ninja -DDRONEACTION_ENABLE_MAVSDK=ON
```

## AppImage

```bash
scripts/package-appimage.sh
```

The AppImage is written to `dist/`. Generated AppImages are not committed to Git.
Each package run removes old AppImage files from `dist/` and writes `dist/droneAction.appimage`.

## GitHub Actions

The workflow at `.github/workflows/build-appimage.yml` builds and tests on `ubuntu-22.04`.

- Pushes and pull requests upload the AppImage as a workflow artifact.
- Tags matching `v*`, such as `v0.1.0`, also publish the AppImage to GitHub Releases.

## RK3588 UDP Command Protocol

Current environment usage is summarized in [docs/ENVIRONMENT_USAGE.md](docs/ENVIRONMENT_USAGE.md).

```json
{
  "version": 1,
  "request_id": "rk-000001",
  "session_id": "control-20260910-001",
  "sequence": 1,
  "command": "forward",
  "value": 1.0,
  "unit": "m/s",
  "duration_ms": 1000,
  "timestamp_ms": 1780732800000
}
```

See [docs/PROTOCOL.md](docs/PROTOCOL.md) for command aliases and safety behavior.
See [docs/APM_USAGE.md](docs/APM_USAGE.md) for ArduCopter SITL and real-aircraft setup.

The `tools/rk3588_command_cli.py` script can simulate an RK3588 sender:

```bash
python3 tools/rk3588_command_cli.py --host 127.0.0.1 --once "前进 1米每秒 2秒"
```

Movement tests send target updates at 10 Hz for the requested duration, followed by `hold`.

For click-based testing, start the browser control panel:

```bash
python3 tools/rk3588_command_panel.py --target-host 127.0.0.1
```

Then open the printed local URL and click the command buttons.
