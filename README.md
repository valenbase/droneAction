# DroneAction

DroneAction is a Linux AppImage target for a PX4 aircraft using a Skydroid G12 control/video link.

The first milestone provides:

- A Qt6/C++ desktop monitoring console.
- Startup run-mode selection for PX4 SITL or real aircraft operation.
- OpenStreetMap-based GPS map view with an aircraft heading marker and local tile cache.
- Interface endpoints are edited from the top settings button and stored under the app config directory.
- Default G12 shared-network endpoints from the local G12 manual:
  - MAVLink telemetry/control: `tcp://192.168.43.1:5760`
  - RTSP video: `rtsp://192.168.144.108:554/stream=0`
- RK3588 UDP JSON command intake on `0.0.0.0:50050`.
- PX4 SITL defaults use MAVLink `udp://:14540` and disable RTSP video.
- A safety gate for movement/high-risk commands.
- GitHub Actions packaging for `x86_64` AppImage releases.

## Local Build

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

Local builds require Qt6, CMake, Ninja, and a C++20 compiler. MAVSDK can be enabled with:

```bash
cmake -S . -B build -G Ninja -DDRONEACTION_ENABLE_MAVSDK=ON
```

## AppImage

```bash
scripts/package-appimage.sh
```

The AppImage is written to `dist/`. Generated AppImages are not committed to Git.

## GitHub Actions

The workflow at `.github/workflows/build-appimage.yml` builds and tests on `ubuntu-22.04`.

- Pushes and pull requests upload the AppImage as a workflow artifact.
- Tags matching `v*`, such as `v0.1.0`, also publish the AppImage to GitHub Releases.

## RK3588 UDP Command Protocol

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

See [docs/PROTOCOL.md](docs/PROTOCOL.md) for command aliases and safety behavior.

The `tools/rk3588_command_cli.py` script can simulate an RK3588 sender:

```bash
python3 tools/rk3588_command_cli.py --host 127.0.0.1 --once "前进 1米每秒 2秒"
```
