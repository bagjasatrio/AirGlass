# Glasscast MVP Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Receiver AirPlay GPL-3 untuk Windows: mirror 1080p/720p dengan decode hardware, audio yang tidak pernah hilang atau telat, dan pemilihan preset eksplisit sebelum mirror.

**Architecture:** Engine C++ hasil fork `AirPlay-Windows`/UxPlay untuk protokol (mDNS native `dnsapi.dll`, pair-verify, FairPlay, AES-CTR H.264, audio RTP). Di atasnya: tangga hwaccel FFmpeg (D3D11VA → NVDEC → QSV → software), renderer D3D11 flip-model, dan pipeline WASAPI dengan audio sebagai master clock. Shell ImGui + tray + CLI memilih preset lewat pre-flight probe.

**Tech Stack:** C++20, CMake ≥3.21, MSVC (VS 2026), FFmpeg prebuilt shared (BtbN gpl-shared), vcpkg (openssl, libplist, sdl2, sdl2-ttf), D3D11 + DXGI 1.4 (`FRAME_LATENCY_WAITABLE_OBJECT`), WASAPI, wlanapi, ImGui, GitHub Actions.

**Spec:** `docs/design/2026-10-22-glasscast-design.md`

## Global Constraints

- Lisensi proyek **GPL-3.0**; setiap file hasil port UxPlay wajib menyebut sumbernya.
- Tanpa dependensi Apple/Bonjour saat runtime. Discovery mDNS lewat `dnsapi.dll` (`DnsServiceRegister`).
- Tanpa installer ber-admin untuk jalan normal. Firewall rule boleh meminta elevation sekali, saat di-klik user.
- Preset kualitas: `720p60`, `1080p30`, `1080p60`, `auto`. Default = pilihan terakhir user, bukan auto.
- Target performa: latency median ≤120 ms, A/V skew ≤20 ms dan tidak tumbuh, RSS <250 MB, CPU ≤15% (hw decode) / ≤60% satu core (software 1080p30).
- Batas bawah: Win10 1809+, 2C/4T SSE4.2, GPU D3D11 FL10_0, 4 GB RAM.
- Tidak ada penurunan kualitas diam-diam: setiap perubahan preset auto harus muncul di UI dengan alasan.

## Review Focus

1. HP keluar dari Wi-Fi saat mirror → sesi harus berakhir dengan pesan jelas, bukan hang di frame terakhir.
2. User mencabut headphone / ganti default audio device saat mirror → audio lanjut di device baru, skew tidak melompat.
3. iPhone mengirim resolusi berbeda dari preset (aspek device) → renderer letterbox tanpa stretch, chip preset tetap menampilkan apa yang benar-benar mengalir.
4. GPU tanpa hw decode di mesin 2C/4T dengan preset 1080p60 → app memberi verdict "Berisiko" sebelum Start, bukan frame-drop setelah mulai.
5. Jaringan 2,4 GHz / AP isolation → self-diagnose menyebut penyebabnya; tidak ada pesan "tidak bisa connect" generik.

---

### Task 1: Scaffold repo + dependency bootstrap

**Files:**
- Create: `CMakeLists.txt`, `README.md`, `LICENSE`, `THIRD_PARTY_NOTICES.md`, `.gitignore`, `cmake/Dependencies.cmake`, `.github/workflows/build.yml`
- Test: `tests/smoke/test_toolchain.py`

**Interfaces:**
- Produces: target CMake `glasscast` (exe), variabel cache `GLASSCAST_FFMPEG_DIR`, `GLASSCAST_VCPKG_TRISTATE`.

- [ ] **Step 1: Tulis test smoke toolchain** — assert CMake ≥3.21, MSVC toolset terdeteksi, file `vendor/ffmpeg/include/libavcodec/avcodec.h` ada.
- [ ] **Step 2: Jalankan, pastikan gagal** — `python tests/smoke/test_toolchain.py` → FAIL karena `vendor/ffmpeg` belum ada.
- [ ] **Step 3: Unduh dependency** — FFmpeg `win64-gpl-shared` ke `vendor/ffmpeg`, `vcpkg` di-bootstrap, `vcpkg install openssl libplist sdl2 sdl2-ttf --triplet x64-windows`.
- [ ] **Step 4: Jalankan test, pastikan pass**.
- [ ] **Step 5: Commit** — `chore: scaffold repo + dependency bootstrap`.

### Task 2: Engine upstream kompilasi & jalan

**Files:**
- Create: `engine/` (vendored `moieric11/AirPlay-Windows`, GPL-3, notices ditambah), `cmake/Engine.cmake`
- Test: `tests/smoke/test_engine_handshake.py`

**Interfaces:**
- Produces: `glasscast.exe --headless --port 7000` yang mengiklankan `_airplay._tcp` dan menjawab `GET /info` dengan bplist.

- [ ] **Step 1: Tulis test** — start exe, tunggu log `mDNS registered`, kirim `GET /info` via HTTP, assert respons diawali `bplist00` dan memuat key `features`.
- [ ] **Step 2: Jalankan, pastikan gagal** (exe belum ada).
- [ ] **Step 3: Vendor source + CMake wiring**, ganti link ke `vendor/ffmpeg` dan vcpkg packages.
- [ ] **Step 4: Build** — `cmake --build build --config Release`; jalankan test → PASS.
- [ ] **Step 5: Commit**.

### Task 3: Probe kapabilitas mesin + jaringan

**Files:**
- Create: `app/probe/Probe.h`, `app/probe/Probe.cpp`, `app/probe/WlanInfo.cpp`
- Test: `tests/app/test_probe.cpp` (Catch2)

**Interfaces:**
- Produces: `struct ProbeResult { bool hw_decode_d3d11va, nvdec, qsv; int cores; uint64_t ram_mb; int wifi_band_mhz; int link_mbps; Verdict verdict(preset); }` dan `ProbeResult ProbeMachine();`

- [ ] **Step 1: Tulis test** — `verdict(Preset::P1080p60)` pada `ProbeResult{hw_decode_d3d11va=false, cores=2, wifi_band_mhz=2437}` → `Verdict::Risky`; `wifi_band_mhz=5180 && hw_decode_d3d11va=true` → `Recommended`.
- [ ] **Step 2: Jalankan, gagal**.
- [ ] **Step 3: Implementasi** — D3D11 device creation + `ID3D11VideoDevice` probe, `WlanQueryInterface(wlan_intf_opcode_current_connection)` untuk band/link rate.
- [ ] **Step 4: Test pass** + jalankan `glasscast --probe` di mesin ini, cetak hasil nyata.
- [ ] **Step 5: Commit**.

### Task 4: Model preset + persistensi

**Files:**
- Create: `app/preset/Preset.h/.cpp`, `app/preset/PresetStore.cpp`
- Test: `tests/app/test_preset.cpp`

**Interfaces:**
- Produces: `enum class Preset { P720p60, P1080p30, P1080p60, Auto }`, `struct PresetSpec { int short_side, fps; }`, `PresetSpec spec_of(Preset)`, `Preset load_last()`, `void save_last(Preset)`.

- [ ] **Step 1: Tulis test** — `spec_of(P1080p30) == {1080, 30}`; simpan `P720p60` lalu `load_last()` mengembalikannya; nilai rusak di file → fallback `P1080p30` dan tandai `used_fallback=true`.
- [ ] **Step 2: Jalankan, gagal**.
- [ ] **Step 3: Implementasi** — JSON kecil di `%APPDATA%\glasscast\settings.json`.
- [ ] **Step 4: Test pass**.
- [ ] **Step 5: Commit**.

### Task 5: Renderer D3D11 flip-model + letterbox

**Files:**
- Create: `app/render/Renderer.h/.cpp`, `app/render/nv12_to_rgb.hlsl`
- Test: `tests/app/test_letterbox.cpp`

**Interfaces:**
- Produces: `class Renderer { bool init(HWND); void present(AVFrame*, PresetSpec); Size last_output(); }`

- [ ] **Step 1: Tulis test** — `compute_letterbox(886,1920, 1920,1080)` → rect `{x=570,y=0,w=780,h=1080}` (integer math, aspect dijaga, tanpa stretch).
- [ ] **Step 2: Jalankan, gagal**.
- [ ] **Step 3: Implementasi** swapchain (`DXGI_SWAP_EFFECT_FLIP_DISCARD`, max frame latency 1) + shader NV12→RGB.
- [ ] **Step 4: Test pass** + jalankan window uji 60 s, catat fps & GPU time.
- [ ] **Step 5: Commit**.

### Task 6: Tangga hardware decode FFmpeg

**Files:**
- Modify: `engine/` decode path → `app/decode/DecoderLadder.h/.cpp`
- Test: `tests/app/test_decode_ladder.cpp`

**Interfaces:**
- Produces: `HwPath select_path(const ProbeResult&)`, `AVCodecContext* open_decoder(HwPath, const AVCodecParameters*)`.

- [ ] **Step 1: Tulis test** — probe palsu `{d3d11va=true, nvdec=true}` → `HwPath::D3D11VA`; semua false → `HwPath::Software`; init D3D11VA gagal → fallback Software dan hasilkan `FallbackEvent`.
- [ ] **Step 2: Jalankan, gagal**.
- [ ] **Step 3: Implementasi** `av_hwdevice_ctx_create` + `get_format` callback + fallback chain.
- [ ] **Step 4: Test pass** + verifikasi di mesin ini bahwa path yang dipilih benar-benar hw (log `hwaccel=d3d11va`).
- [ ] **Step 5: Commit**.

### Task 7: Audio master clock + WASAPI

**Files:**
- Create: `app/audio/AudioClock.h/.cpp`, `app/audio/WasapiSink.h/.cpp`
- Test: `tests/app/test_audio_clock.cpp`

**Interfaces:**
- Produces: `class AudioClock { void on_samples(int64_t pts_us, int n); int64_t audio_position_us(); bool should_present(int64_t video_pts_us) const; }`

- [ ] **Step 1: Tulis test** — video pts 100 ms di belakang posisi audio → `should_present` false (drop); pts dalam ±20 ms → true; skew tidak tumbuh setelah 60 s simulasi drift 0,05%.
- [ ] **Step 2: Jalankan, gagal**.
- [ ] **Step 3: Implementasi** WASAPI event-driven 10 ms, `swr` resample saat rate endpoint ≠ 44100.
- [ ] **Step 4: Test pass**.
- [ ] **Step 5: Commit**.

### Task 8: Tahan ganti audio device

**Files:**
- Modify: `app/audio/WasapiSink.cpp`
- Test: `tests/app/test_device_change.cpp`

**Interfaces:**
- Consumes: `WasapiSink::start/stop` dari Task 7.
- Produces: `WasapiSink::on_default_device_changed()` yang melakukan re-init tanpa menyentuh `AudioClock`.

- [ ] **Step 1: Tulis test** — simulasi 3× device change → clock monoton, tidak ada gap >30 ms yang dilaporkan, `reinit_count == 3`.
- [ ] **Step 2: Jalankan, gagal**.
- [ ] **Step 3: Implementasi** `IMMNotificationClient::OnDefaultDeviceChanged` → rebuild `IAudioClient` di thread audio.
- [ ] **Step 4: Test pass**.
- [ ] **Step 5: Commit**.

### Task 9: UI pre-flight + kartu preset

**Files:**
- Create: `app/ui/PreflightView.cpp`, `app/ui/StatsOverlay.cpp`, `app/main.cpp` (wiring)
- Test: `docs/qa/preflight-checklist.md`

**Interfaces:**
- Consumes: `ProbeResult`, `PresetSpec`, `PresetStore`.
- Produces: `Preset show_preflight(ProbeResult&)` — blokir sampai user memilih; `Auto` menampilkan prediksi pilihannya.

- [ ] **Step 1: Tulis checklist QA manual** — 4 kartu tampil dengan badge, Auto menampilkan "akan memilih: X", preset terakhir terpilih, Start nonaktif sampai probe selesai.
- [ ] **Step 2: Jalankan, pastikan gagal** (UI belum ada).
- [ ] **Step 3: Implementasi** ImGui + tray (Win32 `Shell_NotifyIcon`) + CLI `--preset`, `--stats`.
- [ ] **Step 4: Checklist lulus** + screenshot disimpan di `docs/qa/`.
- [ ] **Step 5: Commit**.

### Task 10: Stats live + self-diagnose jaringan

**Files:**
- Create: `app/diag/NetworkDiagnosis.h/.cpp`, `app/ui/StatsOverlay.cpp` (lanjutan)
- Test: `tests/app/test_diagnosis.cpp`

**Interfaces:**
- Produces: `Diagnosis diagnose(ProbeResult&, MdnsStatus)`, enum alasan: `PublicProfile`, `ApIsolationSuspect`, `VpnActive`, `MdnsBlocked`, `Ok`.

- [ ] **Step 1: Tulis test** — profil Public → `PublicProfile` dengan aksi "ubah ke Private"; adapter VPN aktif + mDNS gagal → `VpnActive` diprioritaskan di atas `MdnsBlocked`.
- [ ] **Step 2: Jalankan, gagal**.
- [ ] **Step 3: Implementasi** cek `NetConnectionProfile`, enumerasi adapter, listener 5353 percobaan, plus tombol "buat rule firewall" via `INetFwPolicy2` (elevation sekali).
- [ ] **Step 4: Test pass** + verifikasi rule benar-benar dibuat di mesin ini (`netsh advfirewall firewall show rule name=Glasscast`).
- [ ] **Step 5: Commit**.

### Task 11: QA audit menyeluruh

**Files:**
- Create: `docs/qa/2026-10-22-qa-audit.md`, `docs/qa/soak-test.py`

**Interfaces:**
- Consumes: seluruh Task 1–10.

- [ ] **Step 1: Build release bersih** — `cmake --build build --config Release` dari nol, catat waktu & warning.
- [ ] **Step 2: Unit test** — `ctest --output-on-failure`, semua hijau.
- [ ] **Step 3: Soak 60 menit** — script mencatat RSS, CPU, drop rate, skew tiap 10 s; gagal kalau RSS >250 MB atau skew >20 ms.
- [ ] **Step 4: Matriks manual** — 4 preset × (hw decode / software) × Wi-Fi 5 GHz, catat latency & hasil; isi tabel di dokumen audit.
- [ ] **Step 5: Audit dokumen** — temuan per kategori (kebenaran, ketahanan, performa, UX, legal/GPL), severity, plus daftar bug yang tidak diperbaiki beserta alasannya.
- [ ] **Step 6: Commit**.
