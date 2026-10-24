// AirGlass — pre-flight preset chooser (ImGui implementation).
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
            return "Softer text, smooth motion. Works on 2.4 GHz.";
        case probe::Preset::P1080p30:
            return "Sharp text, 30 fps. Safe default.";
        case probe::Preset::P1080p60:
            return "Sharp and smooth. Needs GPU decode + 5 GHz.";
        case probe::Preset::Auto:
            return "Picks once from the probe.";
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
    ImGui::SetNextWindowSize(ImVec2(520, 0), ImGuiCond_Appearing);

    bool chose = false;

    // Glass sheet, same material as the chrome: translucent surface, bright rim,
    // no grey title strip. Text stays at full contrast because this panel sits
    // over a dimmed backdrop.
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24, 22));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 22.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 22.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 1.0f);
    // The modal is a decision surface, not decoration: a translucent sheet lets
    // the dashboard text ghost through the preset rows, which reads as a bug.
    ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.0f, 1.0f, 1.0f, 0.62f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.11f, 0.13f, 0.16f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_TextDisabled, ImVec4(0.30f, 0.34f, 0.38f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_TitleBg, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(1, 1, 1, 0.55f));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(1, 1, 1, 0.70f));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.72f, 0.83f, 1.0f, 0.80f));
    ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, ImVec4(0.10f, 0.14f, 0.20f, 0.38f));

    // No AlwaysAutoResize: the long hint strings used to stretch the modal past
    // the window edge. A fixed width plus wrapped text keeps it inside.
    if (ImGui::BeginPopupModal("Choose quality", nullptr,
                               ImGuiWindowFlags_NoSavedSettings |
                                   ImGuiWindowFlags_NoResize |
                                   ImGuiWindowFlags_NoTitleBar)) {
        ImGui::SetWindowFontScale(1.22f);
        ImGui::TextUnformatted("Choose quality");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::Spacing();
        ImGui::TextUnformatted("How sharp should the mirror be?");
        ImGui::PushTextWrapPos(ImGui::GetContentRegionAvail().x);
        ImGui::TextDisabled("%s", probe::explain(probe, probe::Preset::Auto).c_str());
        ImGui::PopTextWrapPos();
        ImGui::Separator();

        const probe::Preset order[] = {probe::Preset::P1080p30, probe::Preset::P1080p60,
                                       probe::Preset::P720p60, probe::Preset::Auto};

        // Full-width clickable rows: the row itself is the affordance, so there
        // is no separate button to mis-align or miss. Three fixed columns keep
        // the verdict badges in a straight line.
        if (ImGui::BeginTable("presets", 3,
                              ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoPadOuterX)) {
            ImGui::TableSetupColumn("preset", ImGuiTableColumnFlags_WidthFixed, 88.0f);
            ImGui::TableSetupColumn("verdict", ImGuiTableColumnFlags_WidthFixed, 96.0f);
            ImGui::TableSetupColumn("detail", ImGuiTableColumnFlags_WidthStretch);

            for (probe::Preset p : order) {
                const probe::Verdict v = probe::verdict_for(probe, p);
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::PushID(static_cast<int>(p));
                if (ImGui::Selectable(preset_title(p), false,
                                      ImGuiSelectableFlags_SpanAllColumns |
                                          ImGuiSelectableFlags_AllowOverlap,
                                      ImVec2(0, 28))) {
                    state.chosen = p;
                    state.open = false;
                    chose = true;
                }

                ImGui::TableSetColumnIndex(1);
                ImGui::AlignTextToFramePadding();
                ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(verdict_color(v)), "%s",
                                   verdict_label(v));

                ImGui::TableSetColumnIndex(2);
                ImGui::AlignTextToFramePadding();
                ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x);
                ImGui::TextDisabled("%s", preset_hint(p));
                if (p == probe::Preset::Auto) {
                    ImGui::TextDisabled("Auto will choose: %s",
                                        probe::preset_name(probe::predicted_preset(probe)));
                }
                ImGui::PopTextWrapPos();
                ImGui::PopID();
            }
            ImGui::EndTable();
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::PushTextWrapPos(ImGui::GetContentRegionAvail().x);
        ImGui::TextDisabled(
            "Preset is locked for the session. Stop and start again to change it.");
        ImGui::PopTextWrapPos();
        if (chose) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    ImGui::PopStyleColor(10);
    ImGui::PopStyleVar(5);
    return chose;
}

}  // namespace ap::ui
