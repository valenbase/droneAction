# Deployment Notes

## G12 Shared Network

The included G12 V2.1 manual describes three PC sharing modes:

- WLAN hotspot
- Type-C Ethernet sharing
- USB network sharing

The default shared telemetry endpoint is `tcp://192.168.43.1:5760`.

The default C10Pro/network camera RTSP endpoint is:

```text
rtsp://<user>:<password>@192.168.144.123/stream=0
```

For third-party cameras, configure the camera gateway as described by Skydroid and update `video_url` in the application settings.

The desktop application uses the UDP transport by default with a 50 ms receive buffer. Frames that arrive
late or remain queued behind a newer frame are discarded, and a stalled stream is reconnected automatically.
The camera should use H.264 or H.265 with a short GOP for the lowest end-to-end delay.

## Build Outputs

GitHub Actions stores AppImage files in two places:

- Workflow artifacts for normal pushes and pull requests.
- GitHub Releases for tags matching `v*`.

The generated file is always named `dist/droneAction.appimage`; each package run removes older AppImage files
from `dist/` before creating the new one.

Do not commit generated `.AppImage` or `.appimage` files.
