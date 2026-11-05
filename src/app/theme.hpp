#pragma once

#include <imgui.h>

#include <initializer_list>

// Win9x-style look, in grayscale only: a silver face, 3D bevels, raised
// buttons that sink when pressed, sunken fields and etched group boxes.
// Every color comes from palette() so the invert setting (a dark variant)
// flips everything at once.
namespace mattrack::ui {

constexpr float kBaseFontSize = 16.0f;
constexpr float kTimerFontSize = 96.0f;

struct Palette {
  ImU32 face;           // window surface
  ImU32 button;         // button surface (a touch lighter than the face)
  ImU32 button_hot;     // button surface under the mouse
  ImU32 highlight;      // outer top-left edge of raised things
  ImU32 light;          // inner top-left edge
  ImU32 shadow;         // inner bottom-right edge
  ImU32 dark_shadow;    // outer bottom-right edge
  ImU32 text;           // text on the face
  ImU32 text_disabled;  // embossed disabled text
  ImU32 field;          // inside of sunken fields (inputs, lists)
  ImU32 well;           // the black "display" wells (timer, heatmap)
  ImU32 well_text;
};

const Palette& palette();
bool inverted();

// Loads the embedded monospace font. Call once after ImGui::CreateContext().
void load_fonts();

// Rebuilds the style from scratch for the given colors and display scale.
// Safe to call again when the scale or invert setting changes.
void apply_theme(bool invert, float display_scale);

// Width in pixels of one bevel line at the current scale.
float px();

enum class Bevel {
  Raised,   // buttons, panels
  Pressed,  // a button held down
  Sunken,   // input fields, wells
  Status,   // shallow inset, status bar cells
  Etched,   // group box outline
};
void draw_bevel(ImDrawList* dl, ImVec2 min, ImVec2 max, Bevel style);

// Raised push button; sinks while held. Hover lightens it and draws the
// dotted focus rectangle. Disabled buttons show embossed gray text.
// `is_default` adds the black outline Win9x used for a dialog's main action.
bool button(const char* label, ImVec2 size = {0, 0}, bool enabled = true,
            bool is_default = false);

// Group box: an etched frame with a caption set into the top edge.
// width <= 0 fills the available width.
void begin_group(const char* caption, float width = 0);
void end_group();

// Sunken black well with large centered text, like an LCD readout.
void lcd_readout(const char* text, ImVec2 size = {0, 0});

// Row of shallow sunken cells pinned to the bottom of the current window.
void status_bar(std::initializer_list<const char*> cells);
float status_bar_height();

void push_timer_font();
void pop_font();

}  // namespace mattrack::ui
