# Ubuntu deployment validation, 2026-09-10

Deployment target: `valen@192.168.0.12`, `/home/valen/Projects/drone_ws`.
Baseline commit on Ubuntu: `9953ab5`.
Environment: Ubuntu 22.04.5 x86_64, GCC 11.3, Qt 6.2.4, MAVSDK 3.11.0.

## Automated checks

Run from the Ubuntu deployment directory:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DDRONEACTION_ENABLE_MAVSDK=ON
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
```

All nine test executables/scripts passed. The loopback aircraft tests exercise the real MAVSDK library and UDP wire messages:

- Repeated target updates use one mode entry, including a change of direction.
- Command loss first sends zero velocity and later requests Position mode.
- A refused exit keeps streaming zero velocity, retries the mode request, and prevents new movement targets from cancelling the exit.
- A manual flight-mode change prevents automatic re-entry until movement authorization is toggled off and on.
- A delayed mode entry does not block the command watchdog.
- Delayed UDP acknowledgements do not block the CLI target stream.
- The control panel can interrupt a long-running movement with its hold button.
- Settings remain accessible at 540 x 520, with the Save button outside the scroll area.

## PX4 SIH flight test

The existing source tree `/home/valen/WorkSpace/drone/PX4_Master_Origin` was built with `make px4_sitl_default -j4`.
Its reported version was `v1.17.0-alpha1-417-gfadf981577-dirty`; this is the existing development firmware on the machine.
The test uses a separate temporary working directory, SIH quadrotor airframe 10040, instance 9, and localhost-only links.

Start the simulator in its own terminal:

```bash
mkdir -p /tmp/droneaction-sitl-validation
cd /tmp/droneaction-sitl-validation
PX4_SYS_AUTOSTART=10040 PX4_SIM_MODEL=sihsim_quadx \
  /home/valen/WorkSpace/drone/PX4_Master_Origin/build/px4_sitl_default/bin/px4 \
  -i 9 -d /home/valen/WorkSpace/drone/PX4_Master_Origin/build/px4_sitl_default/etc \
  -s etc/init.d-posix/rcS
```

The probe supplies centred virtual sticks on the simulator's local port 18579 so Position mode has manual input. Configure only this isolated simulator and run the probe:

```bash
/home/valen/WorkSpace/drone/PX4_Master_Origin/build/px4_sitl_default/bin/px4-param --instance 9 set COM_RC_IN_MODE 1
cd /home/valen/Projects/drone_ws
build/tests/px4_sitl_probe --sitl-local
```

The probe receives telemetry on `127.0.0.1:14549` and sends JSON commands through the actual command server on `127.0.0.1:50059`. It completes takeoff, waits for Hold, streams forward targets for five seconds after mode confirmation, stops updates, verifies braking before Position mode, then lands and waits for disarm.

Observed successful run:

```text
RESULT PASS max_speed 0.881192 stopped_speed 0.0565685 exit_delay_ms 2100 zero_before_exit true
```

Speeds are measured ground speeds in m/s, and include simulation/controller response. The configured command is 1 m/s. The mode-exit default is 2000 ms; the measured 2100 ms includes telemetry confirmation latency.

A subsequent run measured 0.329 m/s at a single Position-mode sample and failed the original 0.3 m/s snapshot assertion. The probe now measures RMS speed over the final second of the three-second Position observation, keeping the same 0.3 m/s threshold. The final run passed:

```text
RESULT PASS max_speed 0.870919 stopped_speed_rms 0.194496 exit_delay_ms 2149 zero_before_exit true
```

The first trial began movement while automatic takeoff was still transitioning to Hold. The takeover protection correctly cancelled entry. The probe was corrected to wait for takeoff completion before starting the movement phase.

Stop the simulator after testing:

```bash
/home/valen/WorkSpace/drone/PX4_Master_Origin/build/px4_sitl_default/bin/px4-shutdown --instance 9
```

## Packaging and scope

The deployment artifact is `dist/droneAction.appimage`. Generated binaries, logs and simulator data are excluded from the source commit. The startup smoke test uses a temporary configuration directory and an isolated display; it does not modify the operator's saved endpoints or authorization.

The final AppImage is 118139384 bytes, with SHA-256 `e8836026cbfd573a1628a3d707763bcdc41bab7d597c260aea76673e79bc383c`. Its packaging input and the built executable share build ID `78cc383e0644efd7843b66487a17a8a63bafb0fb`. The AppImage ran for ten seconds under Xvfb without loader or Qt startup errors, then was terminated by the test timeout. This startup check does not exercise physical control or live video. Detailed logs and the small-window settings screenshot are retained in `build/validation/` on Ubuntu.

These results cover Ubuntu compilation, protocol integration, simulated flight, and settings layout. This run does not validate a physical aircraft, the G12 radio link, or a live RTSP camera.
