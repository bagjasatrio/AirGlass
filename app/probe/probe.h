// Glasscast — machine + network capability probe.
//
// Runs once at startup (and on demand from the pre-flight screen) to answer the
// only question the preset UI needs: can this machine actually sustain
// 720p60 / 1080p30 / 1080p60, or is the user about to pick something that will
// stutter for reasons the app already knew about?
//
// Everything here is local and cheap: a D3D11 device creation, GetSystemInfo,
// GlobalMemoryStatusEx and one WlanQueryInterface call. No network traffic, no
// admin rights, no Apple dependency.
//
// GPL-3.0 (project license). No upstream UxPlay code in this file.

#pragma once

#include <cstdint>
#include <string>

namespace ap::probe {

enum class Verdict {
    Recommended,  // hardware and link both have headroom
    Possible,     // will work, with trade-offs
    Risky,        // expect dropped frames; the UI must say why
};

enum class Preset { P720p60, P1080p30, P1080p60, Auto };

struct Machine {
    bool     d3d11va = false;   // ID3D11VideoDevice available (generic GPU decode)
    bool     nvdec   = false;   // NVIDIA CUDA/NVDEC path usable
    bool     qsv     = false;   // Intel Quick Sync path usable
    bool     hevc_sw = false;   // software HEVC decode is a fallback only; unused in v0.1
    int      cores   = 0;       // logical processors
    uint64_t ram_mb  = 0;
    std::string gpu_name;       // adapter description, for the UI
};

struct Wlan {
    bool     connected   = false;
    bool     is_wifi     = false;
    int      band_mhz    = 0;   // 2437 / 5180 / 5955 (channel centre), 0 = unknown
    int      link_mbps   = 0;   // current receive rate
    std::string ssid;
};

struct ProbeResult {
    Machine machine;
    Wlan    wlan;
};

// Probe the machine (D3D11 video device, cores, RAM) and the active WLAN.
ProbeResult probe();

// Verdict for one preset against one probe result. `Auto` has no verdict of
// its own: it is judged by the preset it predicts (see predicted_preset).
Verdict verdict_for(const ProbeResult& r, Preset p);

// What Auto would pick for this machine right now. Pure function of the probe
// result so the pre-flight screen can show it *before* the user starts.
Preset predicted_preset(const ProbeResult& r);

// Machine-readable reason string for the UI / --probe output, e.g.
// "no hw decode; 2 cores; Wi-Fi 2.4 GHz".
std::string explain(const ProbeResult& r, Preset p);

const char* preset_name(Preset p);
const char* verdict_name(Verdict v);

}  // namespace ap::probe
