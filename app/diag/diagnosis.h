// AirGlass — network self-diagnosis for the "iPhone can't see my PC" case.
//
// The paid apps answer this with a FAQ link. We answer it with the actual
// cause: public firewall profile, VPN adapter stealing the mDNS multicast,
// client isolation on the AP, or a blocked UDP 5353.
//
// GPL-3.0. No upstream UxPlay code in this file.

#pragma once

#include <string>
#include <vector>

#include "probe.h"

namespace ap::diag {

enum class Cause {
    Ok,
    NotConnected,     // no active Wi-Fi/Ethernet association at all
    PublicProfile,    // Windows firewall profile is "Public" -> inbound blocked
    VpnActive,        // a VPN/tunnel adapter is up; mDNS often leaves via it
    MdnsBlocked,      // UDP 5353 bind or loopback multicast check failed
    ApIsolation,      // mDNS went out but no answer: suspect client isolation
};

struct Result {
    Cause cause = Cause::Ok;
    std::string title;    // one line, human
    std::string detail;   // why we think so
    std::string action;   // what to click; empty when nothing to do
    bool fixable = false; // action is wired to a button in the UI
};

// Cheap checks only: network profile, adapter list, UDP 5353 bind, and (when a
// mirror session is already running) whether any mDNS query was answered.
Result diagnose(const probe::ProbeResult& probe, bool mirror_session_active);

const char* cause_name(Cause c);

// Opens the Windows network profile panel. Kept here so the UI does not embed
// shell32 calls.
void open_network_settings();

// Adds an inbound allow rule for the current executable, elevating once.
// Returns true when the rule exists afterwards.
bool ensure_firewall_rule(const std::string& exe_path);

}  // namespace ap::diag
