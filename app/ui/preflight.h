// AirGlass — pre-flight preset chooser.
//
// Shown once, at launch, before any mirror session exists. Four cards, each
// carrying the verdict the probe computed for *this* machine, so the user picks
// with information instead of discovering the ceiling by stuttering.
//
// The golden rule: this screen never changes the answer later. It reports what
// will happen; it does not "optimise" behind the user's back.
//
// GPL-3.0. No upstream UxPlay code in this file.

#pragma once

#include <string>

#include "probe.h"

namespace ap::ui {

struct PreflightState {
    bool          open = true;
    probe::Preset chosen = probe::Preset::P1080p30;
    bool          auto_prediction_ready = false;
};

// Draws the modal. Returns true only on the frame the user confirmed a choice;
// `state.open` is cleared at that point. Safe to call every frame.
bool draw_preflight(PreflightState& state, const probe::ProbeResult& probe);

// Applies a preset to the engine's live settings (canvas size, fps cap,
// hardware decode). Implemented in the renderer translation unit where
// LiveSettings is in scope; declared here so the UI stays engine-agnostic.
struct ApplyTarget;
void apply_preset_to_live(ApplyTarget* target, probe::Preset preset);

}  // namespace ap::ui
