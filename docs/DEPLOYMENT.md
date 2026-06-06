# Deployment Notes

## G12 Shared Network

The included G12 V2.1 manual describes three PC sharing modes:

- WLAN hotspot
- Type-C Ethernet sharing
- USB network sharing

The default shared telemetry endpoint is `tcp://192.168.43.1:5760`.

The default C10Pro/network camera RTSP endpoint is:

```text
rtsp://192.168.144.108:554/stream=0
```

For third-party cameras, configure the camera gateway as described by Skydroid and update `video_url` in the application settings.

## Build Outputs

GitHub Actions stores AppImage files in two places:

- Workflow artifacts for normal pushes and pull requests.
- GitHub Releases for tags matching `v*`.

Do not commit generated `.AppImage` files.
