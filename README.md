# AirGlass

AirPlay receiver for Windows. Free, open source, no session limit, no watermark, no account — and it tells you the truth about what your machine can actually do.

Built because the free tiers of existing receivers cut you off after 15 minutes, quietly drop to 720p, and lose the audio when you plug in headphones.

> **Status: v0.1 work in progress.** The protocol engine is a fork of [`moieric11/AirPlay-Windows`](https://github.com/moieric11/AirPlay-Windows) (itself a UxPlay port). AirGlass adds explicit quality presets, a capability probe that runs before you mirror, a WASAPI audio path that survives device changes, and network self-diagnosis.

## What it does

- **Mirror + audio** from iPhone/iPad to a Windows PC over the same Wi-Fi. No Bonjour install, no firewall fiddling, no Apple runtime.
- **Presets chosen up front** — `720p60`, `1080p30`, `1080p60`, or `auto` — with a verdict per preset computed from *your* hardware and Wi-Fi band before the session starts. Nothing is downgraded silently mid-session.
- **Audio that doesn't disappear** — WASAPI event-driven sink, default-device-change handling, resampling to the endpoint rate, and a backlog guard that drops stale audio rather than letting it drift late.
- **Diagnosis instead of a FAQ link** — if the iPhone can't see the PC, AirGlass names the cause: Public firewall profile, VPN adapter, mDNS port taken, or suspected AP isolation.

## What it does not do

- **No iPhone control.** AirPlay mirroring is view-only; the protocol has no touch injection. (That's what makes Apple's iPhone Mirroring Mac-only.)
- **No DRM content.** Netflix, Disney+, Apple TV+ show black when mirrored — HDCP/FairPlay. Not a bug, not fixable by the receiver.
- **No Miracast or Google Cast.** iPhone doesn't speak Miracast at all.
- **No 4K.** Mirror is capped at ~1080p on the short side; iOS re-renders the screen buffer and H.264-encodes it.

## Usage

```text
airglass.exe                     # start receiver (uses your last preset)
airglass.exe --preset 1080p30    # pick a preset explicitly (saved for next time)
airglass.exe --preset auto       # let the probe choose once, at start
airglass.exe --probe             # print capability report and exit (no server)
airglass.exe --log               # verbose logs to the console
```

On the iPhone: Control Center → Screen Mirroring → your PC name.

### Presets

| Preset | Canvas | FPS | Needs |
|---|---|---|---|
| `720p60` | 720 short side | 60 | any GPU, 2.4 or 5 GHz |
| `1080p30` | 1080 short side | 30 | software decode OK on 4+ cores |
| `1080p60` | 1080 short side | 60 | hardware decode + clean 5 GHz |
| `auto` | — | — | resolved once at start, printed and logged |

Presets are locked for the session: changing one means Stop, then Start. Mid-session renegotiation costs a 1–2 s black frame, which is worse than asking you to reconnect.

## Build

Requirements: Windows 10 1809+ or 11, Visual Studio 2022/2026 with the C++ workload, CMake ≥ 3.21, Git.

```bash
# 1. Dependencies: FFmpeg prebuilt (hardware decoders included) + vcpkg packages
mkdir -p vendor && cd vendor
git clone --depth 1 https://github.com/microsoft/vcpkg.git && ./vcpkg/bootstrap-vcpkg.bat
./vcpkg/vcpkg.exe install openssl libplist sdl2 sdl2-ttf --triplet x64-windows
curl -L -o ffmpeg.zip https://github.com/BtbN/FFmpeg-Builds/releases/latest/download/ffmpeg-master-latest-win64-gpl-shared.zip
powershell -Command "Expand-Archive ffmpeg.zip -DestinationPath ." && mv ffmpeg-master-latest-win64-gpl-shared ffmpeg
cd ..

# 2. Configure + build
cmake -S engine -B build -G "Visual Studio 17 2022" -A x64 \
  -DCMAKE_TOOLCHAIN_FILE=vendor/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release
```

The build links prebuilt FFmpeg instead of compiling it from source (which takes 20–45 minutes); `cmake/FindFFMPEG.cmake` wires the import libraries and resolves the DLL names.

## License

GPL-3.0. The engine is a derivative work of UxPlay / AirPlay-Windows, so the whole project stays GPL-3.0. See [`LICENSE`](LICENSE) and [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md), including the note about the FairPlay blobs.

"AirPlay" is a trademark of Apple Inc. This project is not affiliated with or endorsed by Apple. The name appears here only to describe what the software is compatible with.

"Airglass" is also the name of an unrelated product by Mobeus; this project is independent of it and shares no code or affiliation.
