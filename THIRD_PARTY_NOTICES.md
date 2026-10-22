# Third-Party Notices — Glasscast

Glasscast is GPL-3.0. It is a derivative work of two GPL-3 projects and links
several third-party libraries. Nothing here is relicensed; the notices below
exist so the provenance is auditable.

## 1. Upstream engine

### `engine/` — fork of AirPlay-Windows
- **Origin:** https://github.com/moieric11/AirPlay-Windows (GPL-3.0), itself a
  port/fork of UxPlay.
- **Upstream chain:** UxPlay (https://github.com/FDH2/UxPlay, GPL-3.0) ← RPiPlay
  (unmaintained since 2021) ← the OpenAirPlay protocol notes.
- **What Glasscast changed** (see `git log` for the diffs):
  - `CMakeLists.txt`: links prebuilt FFmpeg via `cmake/FindFFMPEG.cmake`,
    project renamed, Glasscast sources added to the target.
  - `src/main.cpp`: `--preset` and `--probe` handling; preset resolution into
    advertised canvas / fps cap / hwaccel.
  - `src/audio/audio_receiver.{h,cpp}`: audio sink swapped from SDL queue to the
    Glasscast WASAPI sink.
  - `src/audio/audio_output.{h,cpp}`: superseded, excluded from the build.
- **Everything else in `engine/` is upstream code, GPL-3.0, unmodified.**

### `engine/third_party/playfair/`
- **Origin:** https://github.com/EstebanKubata/playfair, also bundled under
  `lib/playfair/` in UxPlay.
- **License:** GPL-3.0 (`engine/third_party/playfair/LICENSE.md`).
- **What it does:** converts the 72-byte `rsaaeskey` from `ANNOUNCE` into the
  AES-128 key used to decrypt audio and mirror payloads.

### `engine/third_party/fairplay_blobs_real.cpp`
- **Origin:** derived from `lib/fairplay_playfair.c` in UxPlay (GPL-3.0). The
  byte sequences were originally captured from an Apple TV responding to
  `POST /fp-setup`.
- **Status:** 568 bytes of Apple-produced data, redistributed on the long-standing
  UxPlay / RPiPlay / shairport-sync precedent. **Apple retains ownership.**
  - Glasscast does not sell, license, or bundle these bytes in any paid product.
  - If Apple asks for removal, remove them: build with
    `engine/third_party/fairplay_blobs_stub.cpp` and the handshake fails at
    `/fp-setup` by design.
  - Legal risk is inherited from upstream and is not increased by Glasscast,
  but it is real: this is the one part of the project that is not
  straightforwardly distributable.

## 2. Linked libraries

| Component | Version source | License | How Glasscast uses it |
|---|---|---|---|
| FFmpeg (prebuilt, BtbN `win64-gpl-shared`) | fetched at build time | LGPL-2.1+ / GPL-2.0+ (gpl build) | decode H.264/HEVC, AAC-ELD, ALAC; libswresample for audio rate conversion |
| OpenSSL | vcpkg | Apache-2.0 | X25519/Ed25519 key agreement, AES-CTR/CBC, SHA |
| libplist | vcpkg | LGPL-2.1 | binary plist encode/decode for `/info`, `SETUP` |
| SDL2 | vcpkg | zlib | window, input, D3D11 renderer backend |
| SDL2_ttf | vcpkg | zlib | metadata text overlay |
| Dear ImGui | FetchContent (docking branch) | MIT | settings overlay, pre-flight preset cards |
| playfair | vendored | GPL-3.0 | FairPlay key derivation (see above) |

FFmpeg is fetched as a **GPL build** on purpose: the engine links it directly,
so the GPL build keeps the licensing story consistent with the project license.

## 3. Trademarks

"AirPlay", "iPhone", "iPad", "Apple TV" are trademarks of Apple Inc. Glasscast
is not affiliated with, endorsed by, or certified by Apple. The terms appear
only to describe compatibility.
