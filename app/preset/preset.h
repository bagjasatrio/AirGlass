// Glasscast — quality presets and their persistence.
// GPL-3.0. No upstream UxPlay code in this file.

#pragma once

#include <string>

#include "probe.h"  // ap::probe::Preset

namespace ap::preset {

using ProbePreset = ::ap::probe::Preset;

struct Spec {
    int short_side = 1080;  // capped short side of the mirror canvas
    int fps        = 30;    // presentation cap; iOS only sends frames it has
};

Spec spec_of(ProbePreset p);

// "1080p30" / "720p60" / "auto". Unknown text yields false.
bool parse(const std::string& text, ProbePreset& out);
std::string to_string(ProbePreset p);

struct Stored {
    ProbePreset last = ProbePreset::P1080p30;
    bool used_fallback = false;  // settings file was missing or corrupt
};

// %APPDATA%\glasscast\settings.ini. Missing or corrupt values fall back to
// 1080p30, which every machine can run.
Stored load();
void save(ProbePreset p);

std::string settings_path();

}  // namespace ap::preset
