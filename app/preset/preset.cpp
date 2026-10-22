// Glasscast — quality presets and their persistence.
// GPL-3.0. No upstream UxPlay code in this file.

#include "preset.h"

#include <windows.h>
#include <shlobj.h>

#include <cstdio>
#include <fstream>
#include <sstream>

namespace ap::preset {
namespace {

constexpr const char* kKeyLast = "last_preset";

bool equal_nocase(const std::string& a, const char* b) {
    return _stricmp(a.c_str(), b) == 0;
}

}  // namespace

Spec spec_of(ProbePreset p) {
    switch (p) {
        case ProbePreset::P720p60:  return Spec{720, 60};
        case ProbePreset::P1080p30: return Spec{1080, 30};
        case ProbePreset::P1080p60: return Spec{1080, 60};
        case ProbePreset::Auto:     return spec_of(ProbePreset::P1080p30);
    }
    return Spec{1080, 30};
}

bool parse(const std::string& text, ProbePreset& out) {
    if (equal_nocase(text, "720p60"))  { out = ProbePreset::P720p60;  return true; }
    if (equal_nocase(text, "1080p30")) { out = ProbePreset::P1080p30; return true; }
    if (equal_nocase(text, "1080p60")) { out = ProbePreset::P1080p60; return true; }
    if (equal_nocase(text, "auto"))    { out = ProbePreset::Auto;     return true; }
    return false;
}

std::string to_string(ProbePreset p) { return ::ap::probe::preset_name(p); }

std::string settings_path() {
    char dir[MAX_PATH]{};
    if (FAILED(SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr, 0, dir))) return {};
    std::string path = dir;
    path += "\\glasscast";
    CreateDirectoryA(path.c_str(), nullptr);
    path += "\\presets.ini";   // separate file: engine owns settings.ini
    return path;
}

Stored load() {
    Stored s;
    const std::string path = settings_path();
    if (path.empty()) {
        s.used_fallback = true;
        return s;
    }
    std::ifstream in(path);
    if (!in) {
        s.used_fallback = true;  // first run: not an error, just no preference yet
        return s;
    }
    std::string line;
    bool found = false;
    while (std::getline(in, line)) {
        const auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string key = line.substr(0, eq);
        const std::string val = line.substr(eq + 1);
        if (key == kKeyLast) {
            ProbePreset p{};
            if (parse(val, p)) {
                s.last = p;
                found = true;
            }
        }
    }
    s.used_fallback = !found;
    return s;
}

void save(ProbePreset p) {
    const std::string path = settings_path();
    if (path.empty()) return;
    // Rewrite the whole file: it holds one key today, and a partial update
    // would need read-modify-write for no benefit.
    std::ofstream out(path, std::ios::trunc);
    if (!out) return;
    out << kKeyLast << '=' << to_string(p) << '\n';
}

}  // namespace ap::preset
