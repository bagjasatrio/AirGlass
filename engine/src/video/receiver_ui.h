#pragma once

#include "imgui.h"
#include <algorithm>
#include <cmath>

// Vector controls use the same renderer as the video; no browser or bitmap icons.
namespace ap::video::receiver_ui {
constexpr ImU32 ink = IM_COL32(32, 38, 41, 255);
constexpr ImU32 muted = IM_COL32(48, 56, 66, 255);
constexpr ImU32 blue = IM_COL32(36, 103, 236, 255);
constexpr ImU32 line = IM_COL32(230, 232, 231, 255);
constexpr ImU32 paper = IM_COL32(245, 246, 244, 255);

// ---------------------------------------------------------------------------
// Liquid glass layer
//
// iOS-style frosted surfaces: translucent fills, a bright hairline on the lit
// edge, a soft cool hairline on the shaded edge, a specular sheen across the
// top, and a diffuse shadow so panels float instead of sitting in boxes.
//
// Honest constraint: ImGui cannot sample the framebuffer, so this is not a true
// backdrop blur. It reads as glass because the surface behind it is a soft
// gradient (see app_background) — translucent panels over smooth colour look
// frosted, while the same panels over hard edges would look merely transparent.
// ---------------------------------------------------------------------------
namespace glass {
constexpr ImU32 fill_strong = IM_COL32(255, 255, 255, 124);
constexpr ImU32 fill_soft   = IM_COL32(255, 255, 255, 84);
constexpr ImU32 fill_faint  = IM_COL32(255, 255, 255, 86);
constexpr ImU32 stroke_hi   = IM_COL32(255, 255, 255, 210);
constexpr ImU32 stroke_lo   = IM_COL32(20, 30, 44, 46);
constexpr ImU32 sheen_top   = IM_COL32(255, 255, 255, 120);
constexpr ImU32 sheen_bot   = IM_COL32(255, 255, 255, 0);
constexpr ImU32 tint_blue   = IM_COL32(190, 218, 255, 158);
constexpr ImU32 tint_green  = IM_COL32(196, 242, 218, 112);
constexpr ImU32 tint_amber  = IM_COL32(255, 232, 188, 130);
constexpr float radius      = 18.0f;
constexpr float radius_sm   = 12.0f;
} // namespace glass

// Soft ambient backdrop: cool vertical gradient plus three colour glows. Kept
// deliberately smooth — it is what makes the translucent panels read as frosted.
inline void app_background(ImDrawList* d, ImVec2 size, ImTextureID noise = 0, float tile = 128.0f) {
    d->AddRectFilledMultiColor(ImVec2(0, 0), size,
                              IM_COL32(233, 239, 252, 255), IM_COL32(228, 232, 250, 255),
                              IM_COL32(209, 217, 238, 255), IM_COL32(214, 221, 241, 255));
    struct Glow { float cx, cy, radius; ImU32 color; };
    const Glow glows[] = {
        {0.10f, 0.00f, 0.78f, IM_COL32(96, 156, 255, 120)},   // cool light, top-left
        {0.94f, 0.06f, 0.66f, IM_COL32(158, 132, 255, 108)},  // violet, top-right
        {0.74f, 0.96f, 0.68f, IM_COL32(58, 214, 196, 78)},    // teal, bottom-right
        {0.26f, 0.90f, 0.52f, IM_COL32(255, 174, 130, 56)},   // warm peach, bottom-left
    };
    for (const Glow& g : glows) {
        const float r_max = g.radius * (size.x > size.y ? size.x : size.y);
        const ImVec2 c(size.x * g.cx, size.y * g.cy);
        // Concentric fills approximate a radial gradient without a shader.
        constexpr int kRings = 34;
        for (int i = kRings; i >= 1; --i) {
            const float t = static_cast<float>(i) / kRings;
            const float r = r_max * t;
            const float falloff = (1.0f - t) * (1.0f - t);
            const int a = static_cast<int>(((g.color >> IM_COL32_A_SHIFT) & 0xFF) * falloff * 0.13f);
            if (a <= 0) continue;
            d->AddCircleFilled(c, r, (g.color & ~IM_COL32_A_MASK) | (static_cast<ImU32>(a) << IM_COL32_A_SHIFT),
                               static_cast<int>(24 + 8 * t));
        }
    }
    // A tiled low-alpha noise layer. A 1-unit step across a wide pastel ramp is
    // visible as banding on 8-bit panels; dithering hides it without a shader.
    if (noise && tile > 8.0f) {
        for (float y = 0; y < size.y; y += tile) {
            for (float x = 0; x < size.x; x += tile) {
                d->AddImage(noise, ImVec2(x, y), ImVec2(x + tile, y + tile));
            }
        }
    }
}

// One frosted surface. `fill` sets the tint; `lift` scales the shadow.
inline void glass_panel(ImDrawList* d, ImVec2 a, ImVec2 b, float round = glass::radius,
                        ImU32 fill = glass::fill_soft, float lift = 1.0f) {
    if (b.x <= a.x || b.y <= a.y) return;
    d->AddRectFilled(ImVec2(a.x, a.y + 1.0f), ImVec2(b.x, b.y + 1.0f),
                     IM_COL32(26, 40, 58, static_cast<int>(10 * lift)), round);
    for (int i = 3; i >= 1; --i) {
        const float o = 3.0f + static_cast<float>(i) * 4.0f;
        const int alpha = static_cast<int>(6.0f * lift * static_cast<float>(4 - i));
        d->AddRectFilled(ImVec2(a.x + 1.0f, a.y + o), ImVec2(b.x - 1.0f, b.y + o),
                         IM_COL32(26, 40, 58, alpha), round + 2.0f);
    }
    d->AddRectFilled(a, b, fill, round);
    const float inset = round * 0.55f;
    const float sheen_h = std::min(30.0f, (b.y - a.y) * 0.45f);
    if (sheen_h > 2.0f && b.x - a.x > inset * 2.0f) {
        d->AddRectFilledMultiColor(ImVec2(a.x + inset, a.y), ImVec2(b.x - inset, a.y + sheen_h),
                                   IM_COL32(255, 255, 255, 165), IM_COL32(255, 255, 255, 165),
                                   glass::sheen_bot, glass::sheen_bot);
    }
    // Bright top rim (light source above) and a cool inner shade at the bottom,
    // so the surface has thickness instead of reading as a flat fill.
    d->AddLine(ImVec2(a.x + round * 0.7f, a.y + 0.7f), ImVec2(b.x - round * 0.7f, a.y + 0.7f),
               IM_COL32(255, 255, 255, 255), 1.8f);
    const float shade_h = std::min(14.0f, (b.y - a.y) * 0.2f);
    if (shade_h > 2.0f && b.x - a.x > inset * 2.0f) {
        d->AddRectFilledMultiColor(ImVec2(a.x + inset, b.y - shade_h), ImVec2(b.x - inset, b.y),
                                   IM_COL32(255, 255, 255, 0), IM_COL32(255, 255, 255, 0),
                                   IM_COL32(28, 42, 62, 26), IM_COL32(28, 42, 62, 26));
    }
    d->AddRect(ImVec2(a.x + 0.5f, a.y + 0.5f), ImVec2(b.x - 0.5f, b.y - 0.5f), glass::stroke_hi, round, 0, 1.0f);
    d->AddRect(a, b, glass::stroke_lo, round, 0, 1.0f);
}

// Small rounded status pill on a glass base.
inline void glass_pill(ImDrawList* d, ImVec2 a, ImVec2 b, ImU32 tint) {
    glass_panel(d, a, b, (b.y - a.y) * 0.5f, tint, 0.45f);
}
enum class Icon { Cast, Home, Music, Screen, Settings, Phone, Previous, Next, Play, Pause, Close, Minimize, Maximize, Fullscreen, Record };

inline void icon(ImDrawList* d, Icon kind, ImVec2 p, float size, ImU32 color) {
    auto v = [&](float x, float y) { return ImVec2(p.x + x * size / 24, p.y + y * size / 24); };
    auto stroke = [&](float x, float y, float xx, float yy) { d->AddLine(v(x,y), v(xx,yy), color, 1.6f); };
    switch (kind) {
    case Icon::Cast:
        stroke(3,15,3,5); stroke(3,5,21,5); stroke(21,5,21,15); stroke(3,15,8,15); stroke(16,15,21,15);
        stroke(12,12,6,20); stroke(6,20,18,20); stroke(18,20,12,12); break;
    case Icon::Home:
        stroke(3,10,12,3); stroke(12,3,21,10); stroke(5,9,5,21); stroke(5,21,10,21);
        stroke(10,21,10,15); stroke(10,15,14,15); stroke(14,15,14,21); stroke(14,21,19,21); stroke(19,21,19,9); break;
    case Icon::Music:
        stroke(9,18,9,5); stroke(9,5,20,3); stroke(20,3,20,16); stroke(9,8,20,6);
        d->AddEllipse(v(6,18), ImVec2(size/8,size/10), color, 0, 20, 1.6f);
        d->AddEllipse(v(17,16), ImVec2(size/8,size/10), color, 0, 20, 1.6f); break;
    case Icon::Screen:
        d->AddRect(v(2,3),v(22,17),color,2,0,1.6f); stroke(12,17,12,21); stroke(8,21,16,21); break;
    case Icon::Settings: {
        // A single closed outline avoids the overlaps and gaps produced by
        // drawing separate teeth on top of a circle at small icon sizes.
        ImVec2 outline[40];
        constexpr float angles[]={-22.5f,-15.f,-11.f,11.f,15.f};
        constexpr float radii[]={7.5f,7.5f,10.f,10.f,7.5f};
        for (int i=0; i<8; ++i) {
            for (int j=0;j<5;++j) {
                const float a=(i*45+angles[j])*3.14159265f/180;
                outline[i*5+j]=v(12+radii[j]*std::cos(a),12+radii[j]*std::sin(a));
            }
        }
        d->AddPolyline(outline,40,color,ImDrawFlags_Closed,1.5f);
        d->AddCircle(v(12,12),size*.13f,color,24,1.5f);
        break;
    }
    case Icon::Phone:
        d->AddRect(v(6,2),v(18,22),color,3,0,1.6f); stroke(10,5,14,5); stroke(11,19,13,19); break;
    case Icon::Previous:
        d->AddTriangleFilled(v(18,5),v(8,12),v(18,19),color); stroke(5,5,5,19); break;
    case Icon::Next:
        d->AddTriangleFilled(v(6,5),v(16,12),v(6,19),color); stroke(19,5,19,19); break;
    case Icon::Play: d->AddTriangleFilled(v(9,5),v(20,12),v(9,19),color); break;
    case Icon::Pause: d->AddRectFilled(v(7,5),v(10,19),color,1); d->AddRectFilled(v(14,5),v(17,19),color,1); break;
    case Icon::Close: stroke(7,7,17,17); stroke(17,7,7,17); break;
    case Icon::Minimize: stroke(7,12,17,12); break;
    case Icon::Maximize: d->AddRect(v(7,7),v(17,17),color,0,0,1.2f); break;
    case Icon::Fullscreen:
        stroke(3,9,3,3); stroke(3,3,9,3); stroke(15,3,21,3); stroke(21,3,21,9);
        stroke(3,15,3,21); stroke(3,21,9,21); stroke(15,21,21,21); stroke(21,21,21,15); break;
    case Icon::Record: d->AddCircleFilled(v(12,12),size*.18f,IM_COL32(205,92,92,255)); break;
    }
}

inline bool surface(const char* id, ImVec2 size) {
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0,0,0,0));
    const bool clicked = ImGui::Button(id, size);
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar();
    return clicked;
}

inline bool icon_button(const char* id, Icon kind, const char* tooltip, float size=35, bool primary=false) {
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const bool clicked = surface(id, ImVec2(size,size));
    auto* d = ImGui::GetWindowDrawList();
    const bool hover = ImGui::IsItemHovered();
    const bool disabled = ImGui::GetStyle().Alpha < .9f;
    ImU32 color = disabled ? IM_COL32(160,168,162,255) : IM_COL32(83,97,88,255);
    if (primary) {
        d->AddCircleFilled(ImVec2(p.x+size/2,p.y+size/2),size/2, disabled ? IM_COL32(174,183,176,255) : hover ? IM_COL32(59,81,68,255) : IM_COL32(40,56,47,255),48);
        d->AddCircle(ImVec2(p.x+size/2,p.y+size/2),size/2,glass::stroke_hi,48,1.2f);
        color = IM_COL32(255,255,255,255);
    } else if (hover) glass_panel(d,p,ImVec2(p.x+size,p.y+size),12.f,glass::fill_soft,0.25f);
    icon(d,kind,ImVec2(p.x+(size-21)/2,p.y+(size-21)/2),21,color);
    if (hover && tooltip) ImGui::SetTooltip("%s",tooltip);
    return clicked;
}

inline bool nav(const char* id, const char* label, Icon kind, bool selected, float width, ImFont* font) {
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const bool clicked = surface(id, ImVec2(width,44));
    auto* d = ImGui::GetWindowDrawList();
    if (selected) glass_panel(d,p,ImVec2(p.x+width,p.y+44),14.f,glass::tint_blue,0.45f);
    else if (ImGui::IsItemHovered()) glass_panel(d,p,ImVec2(p.x+width,p.y+44),14.f,glass::fill_faint,0.2f);
    const ImU32 color = selected ? IM_COL32(28,86,196,255) : IM_COL32(74,82,92,255);
    icon(d,kind,ImVec2(p.x+12,p.y+12),20,color);
    d->AddText(font,font->FontSize,ImVec2(p.x+44,p.y+(44-font->FontSize)/2),color,label);
    return clicked;
}

inline bool text_button(const char* label, ImVec2 size=ImVec2(0,34)) {
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize,1);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,14);
    ImGui::PushStyleColor(ImGuiCol_Border,ImVec4(1,1,1,.62f));
    ImGui::PushStyleColor(ImGuiCol_Button,ImVec4(1,1,1,.46f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,ImVec4(1,1,1,.66f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,ImVec4(1,1,1,.80f));
    const bool pressed = ImGui::Button(label,size);
    ImGui::PopStyleColor(4); ImGui::PopStyleVar(2);
    return pressed;
}

} // namespace ap::video::receiver_ui
