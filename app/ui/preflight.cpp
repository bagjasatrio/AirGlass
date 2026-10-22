// Glasscast — pre-flight preset chooser (ImGui implementation).
// GPL-3.0. No upstream UxPlay code in this file.

#include "ui/preflight.h"

#include "imgui.h"

#include <cstdio>

namespace ap::ui {
namespace {

ImU32 verdict_color(probe::Verdict v) {
    switch (v) {
        case probe::Verdict::Recommended: return IM_COL32(46, 125, 78, 255);
        case probe::Verdict::Possible:    return IM_COL32(150, 110, 30, 255);
        case probe::Verdict::Risky:       return IM_COL32(160, 52, 52, 255);
    }
    return IM_COL32(90, 90, 90, 255);
}

const char* verdict_label(probe::Verdict v) {
    switch (v) {
        case probe::Verdict::Recommended: return "recommended";
        case probe::Verdict::Possible:    return "possible";
        case probe::Verdict::Risky:       return "risky";
    }
    return "possible";
}

const char* preset_title(probe::Preset p) {
    switch (p) {
        case probe::Preset::P720p60:  return "720p60";
        case probe::Preset::P1080p30: return "1080p30";
        case probe::Preset::P1080p60: return "1080p60";
        case probe::Preset::Auto:     return "Auto";
    }
    return "1080p30";
}

const char* preset_hint(probe::Preset p) {
    switch (p) {
        case probe::Preset::P720p60:
            return "Soft text, smooth motion. Survives 2.4 GHz and weak CPUs.";
        case probe::Preset::P1080p30:
            return "Sharp text, 30 fps. The safe default for reading and demos.";
        case probe::Preset::P1080p60:
            return "Sharp and smooth. Needs GPU decode and a clean 5 GHz link.";
        case probe::Preset::Auto:
            return "Picks once from the probe, then only steps down with a reason.";
    }
    return "";
}

}  // namespace

bool draw_preflight(PreflightState& state, const probe::ProbeResult& probe) {
    if (!state.open) return false;

    if (!ImGui::IsPopupOpen("Choose quality")) ImGui::OpenPopup("Choose quality");

    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x * 0.5f,
                                   vp->WorkPos.y + vp->WorkSize.y * 0.5f),
                            ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(560, 0), ImGuiCond_Appearing);

    bool chose = false;
    if (ImGui::BeginPopupModal("Choose quality", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize |
                                   ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::TextUnformatted("How sharp should the mirror be?");
        ImGui::TextDisabled("%s", probe::explain(probe, probe::Preset::Auto).c_str());
        ImGui::Separator();

        const probe::Preset order[] = {probe::Preset::P1080p30, probe::Preset::P1080p60,
                                       probe::Preset::P720p60, probe::Preset::Auto};

        for (probe::Preset p : order) {
            const probe::Verdict v = probe::verdict_for(probe, p);
            ImGui::PushID(static_cast<int>(p));
            ImGui::BeginGroup();
            ImGui::TextUnformatted(preset_title(p));
            ImGui::SameLine(120);
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(verdict_color(v)), "%s",
                               verdict_label(v));
            ImGui::SameLine();
            ImGui::TextDisabled("%s", preset_hint(p));
            if (p == probe::Preset::Auto) {
                ImGui::TextDisabled("Auto will choose: %s",
                                    probe::preset_name(probe::predicted_preset(probe)));
            }
            ImGui::EndGroup();
            ImGui::SameLine(ImGui::GetContentRegionAvail().x - 40);
            if (ImGui::Button("Use", ImVec2(70, 0))) {
                state.chosen = p;
                state.open = false;
                chose = true;
            }
            ImGui::Separator();
            ImGui::PopID();
        }

        ImGui::TextDisabled(
            "Preset is locked for the session. Stop and start again to change it.");
        if (chose) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    return chose;
}

}  // namespace ap::ui
