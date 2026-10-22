// AirGlass — machine + WLAN probe implementation.
// GPL-3.0. No upstream UxPlay code in this file.

#include "probe.h"

#include <windows.h>
#include <d3d11.h>
#include <wlanapi.h>
#include <wrl/client.h>

#include <cstdio>
#include <cstring>
#include <string>

#pragma comment(lib, "wlanapi.lib")
#pragma comment(lib, "d3d11.lib")

namespace ap::probe {
namespace {

using Microsoft::WRL::ComPtr;

constexpr int kBand24 = 2437;   // channel 6 centre
constexpr int kBand5  = 5180;   // channel 36 centre
constexpr int kBand6  = 5955;   // 6 GHz low band

// Adapter vendor IDs. Decode capability is inferred from the vendor because
// FFmpeg's hwaccel list is only authoritative after a device is created; the
// UI needs an answer 500 ms after launch, before any stream exists.
constexpr unsigned kVendorNvidia = 0x10DE;
constexpr unsigned kVendorIntel  = 0x8086;

void probe_machine(Machine& m) {
    SYSTEM_INFO si{};
    GetSystemInfo(&si);
    m.cores = static_cast<int>(si.dwNumberOfProcessors);

    MEMORYSTATUSEX ms{};
    ms.dwLength = sizeof(ms);
    if (GlobalMemoryStatusEx(&ms)) m.ram_mb = ms.ullTotalPhys / (1024ull * 1024ull);

    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0,
                                  D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0};
    D3D_FEATURE_LEVEL got{};
    const HRESULT hr = D3D11CreateDevice(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
        D3D11_CREATE_DEVICE_VIDEO_SUPPORT | D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        levels, ARRAYSIZE(levels), D3D11_SDK_VERSION, &device, &got, &context);
    if (FAILED(hr) || !device) return;

    ComPtr<ID3D11VideoDevice> video_device;
    m.d3d11va = SUCCEEDED(device.As(&video_device)) && video_device != nullptr;

    ComPtr<IDXGIDevice> dxgi;
    ComPtr<IDXGIAdapter> adapter;
    if (SUCCEEDED(device.As(&dxgi)) && SUCCEEDED(dxgi->GetAdapter(&adapter)) && adapter) {
        DXGI_ADAPTER_DESC desc{};
        if (SUCCEEDED(adapter->GetDesc(&desc))) {
            m.nvdec = (desc.VendorId == kVendorNvidia);
            m.qsv   = (desc.VendorId == kVendorIntel);
            // Wide -> narrow for the UI string.
            char narrow[160]{};
            WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, narrow, sizeof(narrow) - 1,
                                nullptr, nullptr);
            m.gpu_name = narrow;
        }
    }
}

void probe_wlan(Wlan& w) {
    HANDLE handle = nullptr;
    DWORD version = 0;
    if (WlanOpenHandle(2, nullptr, &version, &handle) != ERROR_SUCCESS || !handle) return;

    PWLAN_INTERFACE_INFO_LIST list = nullptr;
    if (WlanEnumInterfaces(handle, nullptr, &list) == ERROR_SUCCESS && list) {
        for (DWORD i = 0; i < list->dwNumberOfItems; ++i) {
            const WLAN_INTERFACE_INFO& info = list->InterfaceInfo[i];
            if (info.isState != wlan_interface_state_connected) continue;

            DWORD size = 0;
            void* data = nullptr;
            if (WlanQueryInterface(handle, &info.InterfaceGuid,
                                   wlan_intf_opcode_current_connection, nullptr, &size, &data,
                                   nullptr) == ERROR_SUCCESS &&
                data) {
                auto* conn = static_cast<WLAN_CONNECTION_ATTRIBUTES*>(data);
                const auto& assoc = conn->wlanAssociationAttributes;
                w.connected = true;
                w.is_wifi   = true;
                w.link_mbps = static_cast<int>(assoc.ulRxRate / 1000);  // kbps -> Mbps

                char ssid[33]{};
                std::memcpy(ssid, assoc.dot11Ssid.ucSSID,
                            assoc.dot11Ssid.uSSIDLength > 32 ? 32 : assoc.dot11Ssid.uSSIDLength);
                w.ssid = ssid;

                // Band: the channel-number opcode is the only portable source.
                // Channels 1..14 are 2.4 GHz; anything above is 5 GHz or 6 GHz,
                // which share a numbering range on some drivers, so callers only
                // treat band_mhz as "2.4 vs not-2.4".
                DWORD ch_size = 0;
                void* ch_data = nullptr;
                if (WlanQueryInterface(handle, &info.InterfaceGuid,
                                       wlan_intf_opcode_channel_number, nullptr, &ch_size,
                                       &ch_data, nullptr) == ERROR_SUCCESS &&
                    ch_data && ch_size >= sizeof(ULONG)) {
                    const ULONG channel = *static_cast<ULONG*>(ch_data);
                    w.band_mhz = channel <= 14 ? kBand24 : (channel > 180 ? kBand6 : kBand5);
                    WlanFreeMemory(ch_data);
                } else {
                    w.band_mhz = 0;  // unknown -> treated as "not verified"
                }
                WlanFreeMemory(data);
                break;
            }
        }
        WlanFreeMemory(list);
    }
    WlanCloseHandle(handle, nullptr);
}

bool has_hw_decode(const Machine& m) { return m.d3d11va || m.nvdec || m.qsv; }

}  // namespace

ProbeResult probe() {
    ProbeResult r;
    probe_machine(r.machine);
    probe_wlan(r.wlan);
    return r;
}

Verdict verdict_for(const ProbeResult& r, Preset p) {
    const bool hw = has_hw_decode(r.machine);
    const bool band24 = (r.wlan.band_mhz == kBand24);
    const bool fast_link = r.wlan.connected && r.wlan.link_mbps >= 100;

    switch (p) {
        case Preset::P720p60:
            // Cheap everywhere: ~55 MP/s and 8-12 Mbps.
            if (hw || r.machine.cores >= 4) return Verdict::Recommended;
            return Verdict::Possible;

        case Preset::P1080p30:
            // ~62 MP/s. Software decode is fine on 4+ cores; 2 cores is tight
            // but survivable at 30 fps.
            if (!hw && r.machine.cores < 4) return band24 ? Verdict::Risky : Verdict::Possible;
            if (band24) return Verdict::Possible;
            return Verdict::Recommended;

        case Preset::P1080p60:
            // ~124 MP/s: needs hw decode, and ~25 Mbps needs a clean 5 GHz link.
            if (!hw) return Verdict::Risky;
            if (band24) return Verdict::Risky;
            if (!fast_link) return Verdict::Possible;
            return Verdict::Recommended;

        case Preset::Auto:
            return verdict_for(r, predicted_preset(r));
    }
    return Verdict::Possible;
}

Preset predicted_preset(const ProbeResult& r) {
    const bool hw = has_hw_decode(r.machine);
    const bool band24 = (r.wlan.band_mhz == kBand24);
    const bool fast_link = r.wlan.connected && r.wlan.link_mbps >= 100;

    if (hw && !band24 && fast_link) return Preset::P1080p60;
    if (hw || (r.machine.cores >= 4 && !band24)) return Preset::P1080p30;
    return Preset::P720p60;
}

std::string explain(const ProbeResult& r, Preset p) {
    std::string why;
    const bool hw = has_hw_decode(r.machine);

    if (!hw) {
        why += "no hw decode (";
        why += r.machine.cores;
        why += " cores)";
    } else if (r.machine.nvdec) {
        why += "NVDEC";
    } else if (r.machine.qsv) {
        why += "QuickSync";
    } else {
        why += "D3D11VA";
    }

    why += "; ";
    if (!r.wlan.connected) {
        why += "no Wi-Fi association";
    } else if (r.wlan.band_mhz == kBand24) {
        why += "Wi-Fi 2.4 GHz";
    } else if (r.wlan.band_mhz == kBand5) {
        why += "Wi-Fi 5 GHz";
    } else if (r.wlan.band_mhz == kBand6) {
        why += "Wi-Fi 6 GHz";
    } else {
        why += "Wi-Fi band unknown";
    }
    if (r.wlan.connected) {
        why += " @";
        why += std::to_string(r.wlan.link_mbps);
        why += " Mbps";
    }
    (void)p;
    return why;
}

const char* preset_name(Preset p) {
    switch (p) {
        case Preset::P720p60:  return "720p60";
        case Preset::P1080p30: return "1080p30";
        case Preset::P1080p60: return "1080p60";
        case Preset::Auto:     return "auto";
    }
    return "1080p30";
}

const char* verdict_name(Verdict v) {
    switch (v) {
        case Verdict::Recommended: return "recommended";
        case Verdict::Possible:    return "possible";
        case Verdict::Risky:       return "risky";
    }
    return "possible";
}

}  // namespace ap::probe
