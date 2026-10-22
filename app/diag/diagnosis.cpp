// Glasscast — network self-diagnosis implementation.
// GPL-3.0. No upstream UxPlay code in this file.

#include "diagnosis.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <netlistmgr.h>
#include <netfw.h>
#include <shellapi.h>
#include <wrl/client.h>

#include <algorithm>
#include <cstring>
#include <string>

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ws2_32.lib")

namespace ap::diag {
namespace {

using Microsoft::WRL::ComPtr;

bool com_ready() {
    static const bool ok = SUCCEEDED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED));
    return ok || CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED) == S_FALSE;
}

// True when every connected network is categorised Public — the single most
// common reason an iPhone never sees the PC.
bool public_profile_only(bool& any_connected) {
    any_connected = false;
    if (!com_ready()) return false;
    ComPtr<INetworkListManager> manager;
    if (FAILED(CoCreateInstance(CLSID_NetworkListManager, nullptr, CLSCTX_ALL,
                                IID_PPV_ARGS(&manager))) || !manager) {
        return false;
    }
    VARIANT_BOOL online = VARIANT_FALSE;
    manager->get_IsConnectedToInternet(&online);

    ComPtr<IEnumNetworks> networks;
    if (FAILED(manager->GetNetworks(NLM_ENUM_NETWORK_CONNECTED, &networks)) || !networks) {
        return false;
    }
    bool saw_public = false;
    bool saw_private = false;
    ComPtr<INetwork> network;
    ULONG fetched = 0;
    while (networks->Next(1, &network, &fetched) == S_OK && fetched == 1) {
        any_connected = true;
        NLM_NETWORK_CATEGORY category = NLM_NETWORK_CATEGORY_PUBLIC;
        if (SUCCEEDED(network->GetCategory(&category))) {
            if (category == NLM_NETWORK_CATEGORY_PUBLIC) saw_public = true;
            if (category == NLM_NETWORK_CATEGORY_PRIVATE) saw_private = true;
        }
        network.Reset();
    }
    return saw_public && !saw_private;
}

bool vpn_adapter_up(std::string& name) {
    ULONG size = 16 * 1024;
    std::vector<char> buffer(size);
    auto* addresses = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());
    const ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST |
                        GAA_FLAG_SKIP_DNS_SERVER | GAA_FLAG_INCLUDE_PREFIX;
    if (GetAdaptersAddresses(AF_UNSPEC, flags, nullptr, addresses, &size) == ERROR_BUFFER_OVERFLOW) {
        buffer.resize(size);
        addresses = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());
    }
    if (GetAdaptersAddresses(AF_UNSPEC, flags, nullptr, addresses, &size) != NO_ERROR) return false;

    static const char* kMarkers[] = {"vpn", "wireguard", "tailscale", "zerotier",
                                     "openvpn", "wintun", "tap-windows", "nordlynx"};
    for (auto* a = addresses; a; a = a->Next) {
        if (a->OperStatus != IfOperStatusUp) continue;
        if (a->IfType == IF_TYPE_TUNNEL) {
            char friendly[256]{};
            WideCharToMultiByte(CP_UTF8, 0, a->FriendlyName, -1, friendly, sizeof(friendly) - 1,
                                nullptr, nullptr);
            name = friendly;
            return true;
        }
        char friendly[256]{};
        WideCharToMultiByte(CP_UTF8, 0, a->FriendlyName, -1, friendly, sizeof(friendly) - 1,
                            nullptr, nullptr);
        std::string lower = friendly;
        std::transform(lower.begin(), lower.end(), lower.begin(),
                       [](unsigned char c) { return static_cast<char>(::tolower(c)); });
        for (const char* marker : kMarkers) {
            if (lower.find(marker) != std::string::npos) {
                name = friendly;
                return true;
            }
        }
    }
    return false;
}

// Can we own UDP 5353 and join the mDNS group? Failure means something else on
// this machine (often a broken Bonjour install or another receiver) holds it.
bool mdns_available(std::string& why) {
    SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) {
        why = "socket() failed";
        return false;
    }
    BOOL reuse = TRUE;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(5353);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        why = "UDP 5353 bind failed (another mDNS service holds it)";
        closesocket(s);
        return false;
    }
    ip_mreq mreq{};
    inet_pton(AF_INET, "224.0.0.251", &mreq.imr_multiaddr);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    if (setsockopt(s, IPPROTO_IP, IP_ADD_MEMBERSHIP, reinterpret_cast<const char*>(&mreq),
                   sizeof(mreq)) == SOCKET_ERROR) {
        why = "UDP 5353 multicast join failed";
        closesocket(s);
        return false;
    }
    closesocket(s);
    return true;
}

}  // namespace

Result diagnose(const probe::ProbeResult& probe, bool mirror_session_active) {
    Result r;

    if (!probe.wlan.connected) {
        // Ethernet-only machines report no Wi-Fi association; only flag this as
        // an error when there is also no VPN/tunnel and no mDNS at all.
        std::string vpn_name;
        if (vpn_adapter_up(vpn_name)) {
            r.cause = Cause::VpnActive;
            r.title = "VPN adapter is up";
            r.detail = "\"" + vpn_name + "\" is active; AirPlay discovery often leaves via it.";
            r.action = "Turn the VPN off while mirroring, then reconnect the iPhone.";
            return r;
        }
    }

    bool any_connected = false;
    if (public_profile_only(any_connected)) {
        r.cause = Cause::PublicProfile;
        r.title = "Network profile is Public";
        r.detail = "Windows blocks inbound AirPlay traffic on Public networks.";
        r.action = "Set the network to Private";
        r.fixable = true;
        return r;
    }

    std::string vpn_name;
    if (vpn_adapter_up(vpn_name)) {
        r.cause = Cause::VpnActive;
        r.title = "VPN adapter is up";
        r.detail = "\"" + vpn_name + "\" is active; mDNS multicast usually stops working.";
        r.action = "Turn the VPN off while mirroring.";
        return r;
    }

    std::string mdns_why;
    if (!mdns_available(mdns_why)) {
        r.cause = Cause::MdnsBlocked;
        r.title = "mDNS port 5353 is unavailable";
        r.detail = mdns_why;
        r.action = "Close other AirPlay/Bonjour receivers, or reboot to release the port.";
        return r;
    }

    if (mirror_session_active) {
        r.cause = Cause::ApIsolation;
        r.title = "Router may isolate wireless clients";
        r.detail = "Discovery packets leave the PC but nothing answers. Client/AP isolation "
                   "on the router blocks PC<->iPhone traffic.";
        r.action = "Disable AP/client isolation, or use the USB-C hotspot path.";
        return r;
    }

    r.cause = Cause::Ok;
    r.title = "Network looks ready";
    r.detail = probe::explain(probe, probe::Preset::Auto);
    return r;
}

const char* cause_name(Cause c) {
    switch (c) {
        case Cause::Ok:            return "ok";
        case Cause::NotConnected:  return "not_connected";
        case Cause::PublicProfile: return "public_profile";
        case Cause::VpnActive:     return "vpn_active";
        case Cause::MdnsBlocked:   return "mdns_blocked";
        case Cause::ApIsolation:   return "ap_isolation";
    }
    return "ok";
}

void open_network_settings() {
    ShellExecuteA(nullptr, "open", "ms-settings:network-status", nullptr, nullptr, SW_SHOWNORMAL);
}

bool ensure_firewall_rule(const std::string& exe_path) {
    if (!com_ready()) return false;
    ComPtr<INetFwPolicy2> policy;
    if (FAILED(CoCreateInstance(CLSID_NetFwPolicy2, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&policy))) ||
        !policy) {
        return false;
    }
    ComPtr<INetFwRules> rules;
    if (FAILED(policy->get_Rules(&rules)) || !rules) return false;

    ComPtr<INetFwRule> rule;
    if (FAILED(CoCreateInstance(CLSID_NetFwRule, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&rule))) ||
        !rule) {
        return false;
    }
    BSTR name = SysAllocString(L"Glasscast AirPlay receiver");
    BSTR app = SysAllocString(std::wstring(exe_path.begin(), exe_path.end()).c_str());
    rule->put_Name(name);
    rule->put_ApplicationName(app);
    rule->put_Protocol(NET_FW_IP_PROTOCOL_ANY);
    rule->put_Direction(NET_FW_RULE_DIR_IN);
    rule->put_Action(NET_FW_ACTION_ALLOW);
    rule->put_Enabled(VARIANT_TRUE);
    const long profiles = NET_FW_PROFILE2_PRIVATE | NET_FW_PROFILE2_DOMAIN;
    rule->put_Profiles(profiles);

    const HRESULT hr = rules->Add(rule.Get());
    SysFreeString(name);
    SysFreeString(app);
    return SUCCEEDED(hr) || hr == HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS);
}

}  // namespace ap::diag
