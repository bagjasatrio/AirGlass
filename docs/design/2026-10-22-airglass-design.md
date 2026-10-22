# AirGlass — Design

**Tanggal:** 2026-10-22
**Status:** disetujui pemilik proyek (chat), siap implementasi
**Lisensi:** GPL-3.0 (mewarisi rantai UxPlay)
**Repo:** github.com/<user>/airglass (belum dipublikasikan)

## 1. Intent

Receiver AirPlay untuk Windows yang **gratis tanpa batas waktu**, karena app yang ada memotong sesi di menit ke-15 dan kualitasnya dikorbankan di free tier.

Tiga keluhan asal yang wajib diselesaikan, dan satu-satunya alasan proyek ini ada:

1. **Kualitas gambar jelek** → decode hardware, tanpa downscale diam-diam, tanpa 720p paksa.
2. **Audio kadang hilang / telat** → audio sebagai master clock, tahan ganti device, resampler drift.
3. **UI ribet** → nol prasyarat (tanpa Bonjour, tanpa edit firewall manual), pilih kualitas sebelum mulai, stats jujur.

Patokan sikap produk: **scrcpy**. Satu folder portable, CLI-first, nol prasyarat, stats terbuka.

## 2. Batas bawah perangkat keras (ditetapkan proyek)

| Komponen | Minimum | 1080p60 |
|---|---|---|
| OS | Windows 10 1809+ / 11 | sama |
| CPU | 2C/4T, x86-64 SSE4.2 (Haswell 2013 / Excavator 2015) | 4C+ atau GPU hw decode |
| RAM | 4 GB sistem, app < 250 MB RSS | sama |
| GPU | D3D11 feature level 10_0+ | NVDEC / QSV / D3D11VA |
| Wi-Fi | 802.11n 5 GHz → 1080p30 | 802.11ac 5 GHz, WMM on |

Aturan: **1080p tetap 1080p di semua kelas.** Yang beradaptasi hanya jalur decode dan besar jitter buffer.

## 3. Anggaran performa (kontrak)

| Metrik | Target |
|---|---|
| Latency end-to-end | ≤120 ms median Wi-Fi 5 GHz bersih; ≤80 ms stretch; ≤200 ms worst |
| A/V skew | ≤20 ms, tidak tumbuh seiring waktu |
| CPU | ≤15% total dengan hw decode; ≤60% satu core di software 1080p30 |
| Memori | <250 MB RSS |
| Packet loss | 1% tanpa stutter terlihat |

## 4. Arsitektur

- **Engine C++** — fork `moieric11/AirPlay-Windows` (turunan UxPlay, GPL-3): mDNS via `dnsapi.dll` (tanpa Bonjour), `pair-verify` X25519/Ed25519, FairPlay `fp-setup`, AES-CTR H.264, audio RTP AES-CBC (AAC-ELD / ALAC), NTP timing.
- **Decode** — tangga FFmpeg hwaccel: D3D11VA → NVDEC → QSV → software libavcodec. Probe sekali saat start, cache hasil, re-probe saat init gagal. *(diff inti: upstream masih software decode 100%)*
- **Render** — D3D11 flip-model swapchain, `FRAME_LATENCY_WAITABLE_OBJECT` max 1, shader NV12→RGB, zero-copy dari surface decoder; jalur software lewat staging texture.
- **Audio** — WASAPI event-driven, period 10 ms, **audio sebagai master clock**, `swr` untuk drift + beda sample-rate endpoint, `IMMNotificationClient` untuk ganti device tanpa memutus stream, ring buffer maks 100 ms.
- **Sync** — posisi audio menentukan deadline present video; drop/duplicate frame video boleh, drop audio tidak pernah.
- **Shell** — satu exe portable + DLL, tray + CLI (`--preset`, `--record`, `--stats`, `--port`), window ImGui: pre-flight card, Start/Stop, stats live.

## 5. Preset kualitas (dipilih user, bukan otomatis)

Empat kartu di pre-flight, semuanya dari probe lokal (~0,5 s, tanpa jaringan):

| Preset | Pixel/detik | Bitrate mirror | Decode software | Wi-Fi |
|---|---|---|---|---|
| 720p60 | ~55 MP/s | ~8–12 Mbps | 20–30% satu core | 5 GHz ringan / 2,4 GHz masih bisa |
| 1080p30 | ~62 MP/s | ~10–14 Mbps | 35–50% satu core | 5 GHz |
| 1080p60 | ~124 MP/s | ~18–25 Mbps | ~90–100% satu core → praktis wajib hw decode | 5 GHz 80 MHz bersih |
| **Auto** | — | — | memilih sekali di pre-flight, lalu hanya turun dengan alasan tertulis | — |

Catatan: "1080p" = sisi pendek dibatasi ~1080 dengan aspek device (mis. 886×1920), bukan 1920×1080 letterbox.

### Aturan Auto
1. Memilih **sekali** di pre-flight, pilihannya ditulis di kartu sebelum Start.
2. Selama sesi hanya boleh menyetel knob lokal: jitter buffer, frame pacing.
3. Turun resolusi butuh `TEARDOWN` + `SETUP` (gambar mati ~1–2 s) → hanya setelah knob lokal gagal 10 s berturut-turut (drop >2% atau loss >1,5%), wajib notice + tombol "kunci preset".
4. Naik preset hanya setelah 30 s stabil (histeresis).
5. Chip status selalu menyebut preset nyata: `Auto · 1080p30`.

**Default = pilihan terakhir user**, Auto opt-in eksplisit.

### Fakta yang diakui di UI
- **Bitrate bukan knob kita** — ditentukan sisi iOS dan beradaptasi sendiri. Lever kita: resolusi/fps, buffer, jalur decode, frame pacing. UI tidak boleh menjanjikan "atur kualitas otomatis".
- Preset dikunci selama sesi. Ganti preset = Stop lalu Start.

## 6. Scope

**v0.1 (plan ini):** 1 iPhone, 1080p/720p, audio master clock, pre-flight preset, tray + CLI, stats live, self-diagnose jaringan + rule firewall otomatis.
**v0.2:** rekam MP4 (NVENC/AMF/QSV/CPU), mode USB-C Personal Hotspot, passthrough `-vrtp` ke OBS, preset 720p30.
**Bukan scope:** kontrol iPhone (tak ada touch injection di protokol), konten DRM (HDCP → hitam, final), Miracast / Google Cast, klien Android/macOS.

## 7. Risiko

1. **FairPlay blobs** — 568 byte milik Apple, dipakai atas preseden UxPlay/RPiPlay/shairport-sync. Abu-abu; dipisah ke `THIRD_PARTY_NOTICES`, tidak dimonetisasi.
2. **GPL-3** — fork wajib GPL-3 + source lengkap. Sesuai tujuan open source.
3. **Unsigned exe** — Defender/SmartScreen warning. Portable zip + manifest winget; signing Azure Trusted Signing nanti bila perlu.
4. **Trademark** — "AirPlay" hanya di deskripsi repo, bukan nama produk.

## 8. Non-goal eksplisit

Tidak ada fitur yang mengorbankan kualitas diam-diam. Tidak ada timer, watermark, akun, telemetri, atau installer yang butuh admin untuk jalan normal.
