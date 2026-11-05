#pragma once

#include <imgui.h>

// Black / white / gray only. Every color in the UI comes from here so the
// invert toggle can flip all of them at once.
namespace mattrack::ui {

constexpr float kBaseFontSize = 16.0f;
constexpr float kTimerFontSize = 96.0f;

// Gray level 0 = black, 1 = white, as seen in the normal (white on black)
// scheme. Inverted, every level is mirrored.
ImU32 gray(float level);
ImU32 fg();  // text and borders
ImU32 bg();  // background

bool inverted();

// Loads the embedded monospace font. Call once after ImGui::CreateContext().
void load_fonts();

// Rebuilds the style from scratch for the given colors and display scale.
// Safe to call again when the scale or invert setting changes.
void apply_theme(bool invert, float display_scale);

// Flat bordered button that inverts on hover (white fill, black text).
bool button(const char* label, ImVec2 size = {0, 0}, bool enabled = true);

// Large digits for the countdown.
void push_timer_font();
void pop_font();

}  // namespace mattrack::ui
