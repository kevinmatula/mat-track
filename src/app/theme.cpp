#include "app/theme.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

#include "app/assets.hpp"

namespace mattrack::ui {

namespace {

ImU32 g(float level) {
  const auto v = static_cast<int>(std::lround(level * 255.0f));
  return IM_COL32(v, v, v, 255);
}

ImVec4 g4(ImU32 c) { return ImGui::ColorConvertU32ToFloat4(c); }

// Classic Windows 9x system grays (C0C0C0 face, DFDFDF / 808080 bevels).
const Palette kLight = {
    .face = g(0.753f),
    .button = g(0.831f),
    .button_hot = g(0.863f),
    .highlight = g(1.0f),
    .light = g(0.875f),
    .shadow = g(0.502f),
    .dark_shadow = g(0.04f),
    .text = g(0.0f),
    .text_disabled = g(0.502f),
    .field = g(1.0f),
    .well = g(0.0f),
    .well_text = g(1.0f),
};

// Inverted: the same bevel logic on a dark face.
const Palette kDark = {
    .face = g(0.20f),
    .button = g(0.26f),
    .button_hot = g(0.30f),
    .highlight = g(0.55f),
    .light = g(0.33f),
    .shadow = g(0.10f),
    .dark_shadow = g(0.0f),
    .text = g(1.0f),
    .text_disabled = g(0.45f),
    .field = g(0.0f),
    .well = g(0.0f),
    .well_text = g(1.0f),
};

bool g_inverted = false;
// Two scales: g_px is whole pixels, for bevels and lines (so they stay
// crisp); g_s is the exact display scale, for sizes, padding and gaps.
float g_px = 1.0f;
float g_s = 1.0f;

struct Group {
  ImVec2 start;
  float width;
  float min_height;
  const char* caption;
};
std::vector<Group> g_groups;

// The open toolbar strip.
struct Toolbar {
  ImVec2 start;
  float width;
};
Toolbar g_toolbar;

// The open list view's column layout and outer rectangle.
struct List {
  std::vector<float> x;  // column starts, relative to the row
  std::vector<float> w;
  ImVec2 min;
  float header_h;
};
List g_list;

const char* label_end(const char* label) {
  const char* hashes = std::strstr(label, "##");
  return hashes ? hashes : label + std::strlen(label);
}

// Filled strip helpers so 1px lines land exactly on pixels at any scale.
void hline(ImDrawList* dl, float x0, float x1, float y, ImU32 c) {
  dl->AddRectFilled({x0, y}, {x1, y + g_px}, c);
}
void vline(ImDrawList* dl, float x, float y0, float y1, ImU32 c) {
  dl->AddRectFilled({x, y0}, {x + g_px, y1}, c);
}

// One ring: top/left in `tl`, bottom/right in `br`.
void ring(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 tl, ImU32 br) {
  hline(dl, a.x, b.x - g_px, a.y, tl);
  vline(dl, a.x, a.y, b.y - g_px, tl);
  hline(dl, a.x, b.x, b.y - g_px, br);
  vline(dl, b.x - g_px, a.y, b.y, br);
}

// Win9x focus cue: a 1px dotted rectangle.
void dotted_rect(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 c) {
  const float step = 2 * g_px;
  for (float x = a.x; x < b.x; x += step) {
    hline(dl, x, std::min(x + g_px, b.x), a.y, c);
    hline(dl, x, std::min(x + g_px, b.x), b.y - g_px, c);
  }
  for (float y = a.y; y < b.y; y += step) {
    vline(dl, a.x, y, std::min(y + g_px, b.y), c);
    vline(dl, b.x - g_px, y, std::min(y + g_px, b.y), c);
  }
}

// Disabled text, Win9x style: gray text over a highlight copy offset 1px.
void embossed_text(ImDrawList* dl, ImVec2 pos, const char* begin, const char* end) {
  const Palette& p = palette();
  dl->AddText({pos.x + g_px, pos.y + g_px}, p.highlight, begin, end);
  dl->AddText(pos, p.text_disabled, begin, end);
}

ImVec2 snap(ImVec2 v) { return {std::floor(v.x), std::floor(v.y)}; }

}  // namespace

const Palette& palette() { return g_inverted ? kDark : kLight; }
bool inverted() { return g_inverted; }
float px() { return g_px; }
float scale() { return g_s; }

void load_fonts() {
  ImGuiIO& io = ImGui::GetIO();
  ImFontConfig cfg;
  cfg.FontDataOwnedByAtlas = false;  // static embedded data; don't free it
  std::strncpy(cfg.Name, "IBM Plex Mono", sizeof cfg.Name - 1);
  // ImGui only reads the font data, but the API takes a non-const pointer.
  io.Fonts->AddFontFromMemoryTTF(const_cast<unsigned char*>(assets::font_ttf),
                                 static_cast<int>(assets::font_ttf_size), kBaseFontSize, &cfg);
}

void apply_theme(bool invert, float display_scale) {
  g_inverted = invert;
  g_px = std::max(1.0f, std::round(display_scale));
  g_s = display_scale;
  const Palette& p = palette();

  ImGuiStyle& style = ImGui::GetStyle();
  style = ImGuiStyle();

  style.WindowPadding = {16, 14};
  style.FramePadding = {10, 5};
  style.ItemSpacing = {8, 8};
  style.ItemInnerSpacing = {6, 6};
  style.ScrollbarSize = 16;
  style.GrabMinSize = 16;

  style.WindowRounding = 0;
  style.ChildRounding = 0;
  style.FrameRounding = 0;
  style.PopupRounding = 0;
  style.ScrollbarRounding = 0;
  style.GrabRounding = 0;
  style.TabRounding = 0;

  style.WindowBorderSize = 1;
  style.ChildBorderSize = 1;
  style.PopupBorderSize = 1;
  style.FrameBorderSize = 0;  // frames get drawn bevels instead
  style.TabBorderSize = 1;

  style.FontSizeBase = kBaseFontSize;
  style.ScaleAllSizes(display_scale);
  style.FontScaleDpi = display_scale;

  ImVec4* c = style.Colors;
  for (int i = 0; i < ImGuiCol_COUNT; ++i) c[i] = g4(p.face);

  c[ImGuiCol_Text] = g4(p.text);
  c[ImGuiCol_TextDisabled] = g4(p.text_disabled);
  c[ImGuiCol_WindowBg] = g4(p.face);
  c[ImGuiCol_ChildBg] = g4(p.face);
  c[ImGuiCol_PopupBg] = g4(p.face);
  c[ImGuiCol_Border] = g4(p.shadow);
  c[ImGuiCol_BorderShadow] = {0, 0, 0, 0};

  // Inputs and combo boxes are white fields.
  c[ImGuiCol_FrameBg] = g4(p.field);
  c[ImGuiCol_FrameBgHovered] = g4(p.field);
  c[ImGuiCol_FrameBgActive] = g4(p.field);

  // Stock ImGui buttons (rarely used; ui::button() draws the real thing).
  c[ImGuiCol_Button] = g4(p.button);
  c[ImGuiCol_ButtonHovered] = g4(p.button_hot);
  c[ImGuiCol_ButtonActive] = g4(p.shadow);

  // Selection in lists/combos: a mid gray band that keeps the text readable.
  c[ImGuiCol_Header] = g4(p.light);
  c[ImGuiCol_HeaderHovered] = g4(p.light);
  c[ImGuiCol_HeaderActive] = g4(p.shadow);

  c[ImGuiCol_CheckMark] = g4(p.text);
  c[ImGuiCol_SliderGrab] = g4(p.face);
  c[ImGuiCol_SliderGrabActive] = g4(p.light);
  c[ImGuiCol_ScrollbarBg] = g4(p.light);
  c[ImGuiCol_ScrollbarGrab] = g4(p.face);
  c[ImGuiCol_ScrollbarGrabHovered] = g4(p.face);
  c[ImGuiCol_ScrollbarGrabActive] = g4(p.shadow);
  c[ImGuiCol_Separator] = g4(p.shadow);
  c[ImGuiCol_SeparatorHovered] = g4(p.shadow);
  c[ImGuiCol_SeparatorActive] = g4(p.dark_shadow);
  c[ImGuiCol_TextSelectedBg] = g4(p.shadow);
  c[ImGuiCol_NavCursor] = g4(p.text);
  c[ImGuiCol_ModalWindowDimBg] = {0, 0, 0, 0.35f};
}

void draw_bevel(ImDrawList* dl, ImVec2 min, ImVec2 max, Bevel style) {
  const Palette& p = palette();
  const ImVec2 a = snap(min);
  const ImVec2 b = snap(max);
  const ImVec2 a2 = {a.x + g_px, a.y + g_px};
  const ImVec2 b2 = {b.x - g_px, b.y - g_px};
  switch (style) {
    case Bevel::Raised:
      ring(dl, a, b, p.highlight, p.dark_shadow);
      ring(dl, a2, b2, p.light, p.shadow);
      break;
    case Bevel::Pressed:
      ring(dl, a, b, p.dark_shadow, p.highlight);
      ring(dl, a2, b2, p.shadow, p.light);
      break;
    case Bevel::Sunken:
      ring(dl, a, b, p.shadow, p.highlight);
      ring(dl, a2, b2, p.dark_shadow, p.light);
      break;
    case Bevel::Status:
      ring(dl, a, b, p.shadow, p.highlight);
      break;
    case Bevel::Etched:
      ring(dl, a, b, p.shadow, p.highlight);
      ring(dl, a2, b2, p.highlight, p.shadow);
      break;
  }
}

bool button(const char* label, ImVec2 size, bool enabled, bool is_default) {
  const char* end = label_end(label);
  const ImVec2 text = ImGui::CalcTextSize(label, end);
  const ImGuiStyle& style = ImGui::GetStyle();
  // Win9x buttons are a little wider than their caption.
  if (size.x <= 0) size.x = std::max(text.x + style.FramePadding.x * 2, 75.0f * g_s);
  if (size.y <= 0) size.y = text.y + style.FramePadding.y * 2;

  const ImVec2 min = snap(ImGui::GetCursorScreenPos());
  const ImVec2 max = {min.x + std::floor(size.x), min.y + std::floor(size.y)};

  ImGui::BeginDisabled(!enabled);
  const bool clicked = ImGui::InvisibleButton(label, size);
  const bool hovered = enabled && ImGui::IsItemHovered();
  const bool held = enabled && ImGui::IsItemActive() && hovered;
  ImGui::EndDisabled();

  const Palette& p = palette();
  ImDrawList* dl = ImGui::GetWindowDrawList();
  ImVec2 bmin = min;
  ImVec2 bmax = max;
  if (is_default && enabled) {
    ring(dl, min, max, p.dark_shadow, p.dark_shadow);
    bmin = {min.x + g_px, min.y + g_px};
    bmax = {max.x - g_px, max.y - g_px};
  }
  dl->AddRectFilled(bmin, bmax, hovered ? p.button_hot : p.button);
  draw_bevel(dl, bmin, bmax, held ? Bevel::Pressed : Bevel::Raised);

  const float shift = held ? g_px : 0.0f;
  const ImVec2 pos = snap({min.x + (size.x - text.x) * 0.5f + shift,
                           min.y + (size.y - text.y) * 0.5f + shift});
  if (enabled) {
    dl->AddText(pos, p.text, label, end);
  } else {
    embossed_text(dl, pos, label, end);
  }
  if (hovered) {
    const float inset = 4 * g_s;
    dotted_rect(dl, {min.x + inset + shift, min.y + inset + shift},
                {max.x - inset + shift, max.y - inset + shift}, p.text);
  }
  return clicked && enabled;
}

bool toggle_button(const char* label, bool active, ImVec2 size) {
  const char* end = label_end(label);
  const ImVec2 text = ImGui::CalcTextSize(label, end);
  const ImGuiStyle& style = ImGui::GetStyle();
  if (size.x <= 0) size.x = text.x + style.FramePadding.x * 2;
  if (size.y <= 0) size.y = text.y + style.FramePadding.y * 2;

  const ImVec2 min = snap(ImGui::GetCursorScreenPos());
  const ImVec2 max = {min.x + std::floor(size.x), min.y + std::floor(size.y)};
  const bool clicked = ImGui::InvisibleButton(label, size);
  const bool hovered = ImGui::IsItemHovered();
  const bool down = active || (ImGui::IsItemActive() && hovered);

  const Palette& p = palette();
  ImDrawList* dl = ImGui::GetWindowDrawList();
  dl->AddRectFilled(min, max, active ? p.light : (hovered ? p.button_hot : p.button));
  draw_bevel(dl, min, max, down ? Bevel::Pressed : Bevel::Raised);
  const float shift = down ? g_px : 0.0f;
  dl->AddText(snap({min.x + (size.x - text.x) * 0.5f + shift,
                    min.y + (size.y - text.y) * 0.5f + shift}),
              p.text, label, end);
  return clicked;
}

void begin_group(const char* caption, float width, float min_height) {
  const ImVec2 start = snap(ImGui::GetCursorScreenPos());
  if (width <= 0) width = avail_width();
  g_groups.push_back({start, std::floor(width), min_height, caption});
  ImGui::BeginGroup();  // the whole box, so SameLine() works after it

  const ImGuiStyle& style = ImGui::GetStyle();
  // Leave room for the caption line, then indent the contents.
  ImGui::Dummy({0, ImGui::GetTextLineHeight() + style.ItemSpacing.y * 0.5f});
  ImGui::Indent(style.WindowPadding.x);
  ImGui::BeginGroup();
  ImGui::PushItemWidth(width - 2 * style.WindowPadding.x);
}

float avail_width() {
  if (g_groups.empty()) return ImGui::GetContentRegionAvail().x;
  const Group& grp = g_groups.back();
  const float inner_right = grp.start.x + grp.width - ImGui::GetStyle().WindowPadding.x;
  return std::max(0.0f, inner_right - ImGui::GetCursorScreenPos().x);
}

void end_group() {
  IM_ASSERT(!g_groups.empty());
  const Group grp = g_groups.back();
  g_groups.pop_back();

  ImGui::PopItemWidth();
  ImGui::EndGroup();
  const ImGuiStyle& style = ImGui::GetStyle();
  ImGui::Unindent(style.WindowPadding.x);
  const float bottom = std::floor(std::max(ImGui::GetItemRectMax().y + style.WindowPadding.y,
                                           grp.start.y + grp.min_height));
  ImGui::SetCursorScreenPos({grp.start.x, bottom});
  ImGui::Dummy({grp.width, style.ItemSpacing.y * 0.5f});
  ImGui::EndGroup();

  const Palette& p = palette();
  ImDrawList* dl = ImGui::GetWindowDrawList();
  const float line_h = ImGui::GetTextLineHeight();
  const float frame_top = std::floor(grp.start.y + line_h * 0.5f);
  draw_bevel(dl, {grp.start.x, frame_top}, {grp.start.x + grp.width, bottom}, Bevel::Etched);

  // Caption sits on the top edge, on a patch of face color.
  const char* end = label_end(grp.caption);
  const ImVec2 text = ImGui::CalcTextSize(grp.caption, end);
  const ImVec2 pos = {grp.start.x + 8 * g_s, grp.start.y};
  dl->AddRectFilled({pos.x - 2 * g_px, pos.y}, {pos.x + text.x + 2 * g_px, pos.y + text.y},
                    p.face);
  dl->AddText(pos, p.text, grp.caption, end);
}

bool begin_combo(const char* label, const char* preview, bool enabled) {
  const Palette& p = palette();
  // BeginCombo switches to the popup window when it opens, so anything
  // pushed here must be popped in the window it was pushed in: the disabled
  // state right after BeginCombo (a disabled combo never opens), the popup
  // color only after EndCombo.
  if (!enabled) ImGui::BeginDisabled();
  // The drop-down list is a white field, unlike dialogs.
  ImGui::PushStyleColor(ImGuiCol_PopupBg, g4(p.field));
  const bool open = ImGui::BeginCombo(label, preview);
  if (!enabled) ImGui::EndDisabled();
  if (!open) ImGui::PopStyleColor();

  ImDrawList* dl = ImGui::GetWindowDrawList();
  const ImVec2 min = ImGui::GetItemRectMin();
  const ImVec2 fmax = ImGui::GetItemRectMax();
  draw_bevel(dl, min, fmax, Bevel::Sunken);
  // Raised arrow button inside the right end of the field.
  const float h = fmax.y - min.y;
  draw_bevel(dl, {fmax.x - h, min.y + 2 * g_px}, {fmax.x - 2 * g_px, fmax.y - 2 * g_px},
             open ? Bevel::Pressed : Bevel::Raised);
  return open;
}

void end_combo() {
  ImGui::EndCombo();
  ImGui::PopStyleColor();  // pushed by begin_combo
}

bool input_text(const char* label, char* buf, std::size_t size, float width) {
  if (width > 0) ImGui::SetNextItemWidth(width);
  const bool enter =
      ImGui::InputText(label, buf, size, ImGuiInputTextFlags_EnterReturnsTrue);
  draw_bevel(ImGui::GetWindowDrawList(), ImGui::GetItemRectMin(), ImGui::GetItemRectMax(),
             Bevel::Sunken);
  return enter;
}

void begin_list(const char* id, std::initializer_list<ListColumn> columns, ImVec2 size) {
  const Palette& p = palette();
  ImDrawList* dl = ImGui::GetWindowDrawList();
  const ImGuiStyle& style = ImGui::GetStyle();
  const ImVec2 min = snap(ImGui::GetCursorScreenPos());
  const float inner_w = size.x - 4 * g_px;  // inside the sunken bevel

  // Column layout: fixed widths first, the rest shared by stretch columns.
  float fixed = 0;
  int stretch = 0;
  for (const auto& c : columns) {
    if (c.width > 0) fixed += c.width;
    else ++stretch;
  }
  const float scrollbar = style.ScrollbarSize;
  const float share = stretch ? std::max(0.0f, inner_w - scrollbar - fixed) / stretch : 0;
  g_list.x.clear();
  g_list.w.clear();
  float x = 0;
  for (const auto& c : columns) {
    g_list.x.push_back(x);
    g_list.w.push_back(c.width > 0 ? c.width : share);
    x += g_list.w.back();
  }

  // Raised header cells inside the top of the field.
  const float header_h = ImGui::GetTextLineHeight() + 6 * g_s;
  g_list.min = min;
  g_list.header_h = header_h;
  const float hx = min.x + 2 * g_px;
  const float hy = min.y + 2 * g_px;
  std::size_t i = 0;
  for (const auto& c : columns) {
    const float cx = std::floor(hx + g_list.x[i]);
    float cw = std::floor(g_list.w[i]);
    if (i + 1 == columns.size()) cw = std::floor(min.x + size.x - 2 * g_px - cx);  // to the edge
    dl->AddRectFilled({cx, hy}, {cx + cw, hy + header_h}, p.face);
    draw_bevel(dl, {cx, hy}, {cx + cw, hy + header_h}, Bevel::Raised);
    dl->AddText({cx + 6 * g_s, hy + 3 * g_s}, p.text, c.title);
    ++i;
  }

  ImGui::SetCursorScreenPos({min.x + 2 * g_px, hy + header_h});
  ImGui::PushStyleColor(ImGuiCol_ChildBg, g4(p.field));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 2 * g_px});
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {style.ItemSpacing.x, 2 * g_px});
  ImGui::BeginChild(id, {inner_w, size.y - header_h - 4 * g_px}, ImGuiChildFlags_None);
}

float list_column_x(int column) { return g_list.x[static_cast<std::size_t>(column)]; }
float list_column_width(int column) { return g_list.w[static_cast<std::size_t>(column)]; }

void end_list() {
  ImGui::EndChild();
  ImGui::PopStyleVar(2);
  ImGui::PopStyleColor();
  const ImVec2 max = {ImGui::GetItemRectMax().x + 2 * g_px, ImGui::GetItemRectMax().y + 2 * g_px};
  draw_bevel(ImGui::GetWindowDrawList(), g_list.min, max, Bevel::Sunken);
  ImGui::SetCursorScreenPos({g_list.min.x, max.y});
  ImGui::Dummy({max.x - g_list.min.x, 0});
}

void check_mark(bool checked) {
  const Palette& p = palette();
  const float size = std::floor(ImGui::GetTextLineHeight() * 0.8f);
  const ImVec2 pos = ImGui::GetCursorScreenPos();
  const ImVec2 a = snap({pos.x, pos.y + (ImGui::GetTextLineHeight() - size) * 0.5f});
  const ImVec2 b = {a.x + size, a.y + size};
  ImDrawList* dl = ImGui::GetWindowDrawList();
  dl->AddRectFilled(a, b, p.field);
  draw_bevel(dl, a, b, Bevel::Sunken);
  if (checked) {
    // The classic 3-pixel-thick check, drawn as two strokes.
    const float u = size / 12.0f;
    const ImVec2 pts[3] = {{a.x + 3 * u, a.y + 6 * u}, {a.x + 5 * u, a.y + 8.5f * u},
                           {a.x + 9.5f * u, a.y + 3.5f * u}};
    dl->AddPolyline(pts, 3, p.text, 2 * g_px, ImDrawFlags_None);
  }
  ImGui::Dummy({size, ImGui::GetTextLineHeight()});
}

bool checkbox(const char* label, bool* value) {
  const Palette& p = palette();
  ImGui::PushStyleColor(ImGuiCol_FrameBg, g4(p.field));
  ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, g4(p.field));
  ImGui::PushStyleColor(ImGuiCol_FrameBgActive, g4(p.light));
  const bool changed = ImGui::Checkbox(label, value);
  ImGui::PopStyleColor(3);
  // The box is the square at the start of the item.
  const ImVec2 a = ImGui::GetItemRectMin();
  const float h = ImGui::GetFrameHeight();
  draw_bevel(ImGui::GetWindowDrawList(), a, {a.x + h, a.y + h}, Bevel::Sunken);
  return changed;
}

namespace {

// Shared by the number fields: a sunken frame around whatever ImGui drew.
void sunken_last_item() {
  draw_bevel(ImGui::GetWindowDrawList(), ImGui::GetItemRectMin(), ImGui::GetItemRectMax(),
             Bevel::Sunken);
}

}  // namespace

bool input_int(const char* label, int* value, int min, int max, float width) {
  ImGui::SetNextItemWidth(width);
  int v = *value;
  ImGui::InputInt(label, &v, 0, 0);
  sunken_last_item();
  v = std::clamp(v, min, max);
  const bool changed = v != *value;
  *value = v;
  return changed;
}

bool input_minutes(const char* label, double* value, double min, double max, float width) {
  ImGui::SetNextItemWidth(width);
  double v = *value;
  ImGui::InputDouble(label, &v, 0, 0, "%g");
  sunken_last_item();
  v = std::clamp(v, min, max);
  const bool changed = v != *value;
  *value = v;
  return changed;
}

void progress_bar(float fraction, ImVec2 size) {
  const Palette& p = palette();
  ImDrawList* dl = ImGui::GetWindowDrawList();
  const ImVec2 a = snap(ImGui::GetCursorScreenPos());
  const ImVec2 b = {a.x + std::floor(size.x), a.y + std::floor(size.y)};
  dl->AddRectFilled(a, b, p.field);
  draw_bevel(dl, a, b, Bevel::Sunken);

  // Blocks, as in the classic progress bar, inside a 2px margin.
  const float inner_x0 = a.x + 3 * g_px;
  const float inner_x1 = b.x - 3 * g_px;
  const float bh = (b.y - a.y) - 6 * g_s;
  const float block = std::max(4 * g_px, std::floor(bh * 0.6f));
  const float gap = 2 * g_px;
  const float filled = inner_x0 + (inner_x1 - inner_x0) * std::clamp(fraction, 0.0f, 1.0f);
  for (float x = inner_x0; x + block <= filled + 0.5f; x += block + gap) {
    // Text color: dark on the white field, light on the inverted black one.
    dl->AddRectFilled({x, a.y + 3 * g_px}, {x + block, b.y - 3 * g_px}, p.text);
  }
  ImGui::Dummy(size);
}

void begin_toolbar(float width) {
  g_toolbar = {snap(ImGui::GetCursorScreenPos()), std::floor(width)};
  ImGui::BeginGroup();
  ImGui::SetCursorScreenPos({g_toolbar.start.x + 8 * g_s, g_toolbar.start.y + 4 * g_s});
  ImGui::BeginGroup();
}

void end_toolbar() {
  ImGui::EndGroup();
  const float bottom = std::floor(ImGui::GetItemRectMax().y + 4 * g_s);
  const ImVec2 a = g_toolbar.start;
  const ImVec2 b = {a.x + g_toolbar.width, bottom};
  draw_bevel(ImGui::GetWindowDrawList(), a, b, Bevel::Raised);
  ImGui::SetCursorScreenPos({a.x, b.y});
  ImGui::Dummy({g_toolbar.width, 0});
  ImGui::EndGroup();
}

void toolbar_separator() {
  const float h = ImGui::GetFrameHeight();
  const ImVec2 pos = snap(ImGui::GetCursorScreenPos());
  const Palette& p = palette();
  ImDrawList* dl = ImGui::GetWindowDrawList();
  dl->AddRectFilled({pos.x + 4 * g_px, pos.y}, {pos.x + 5 * g_px, pos.y + h}, p.shadow);
  dl->AddRectFilled({pos.x + 5 * g_px, pos.y}, {pos.x + 6 * g_s, pos.y + h}, p.highlight);
  ImGui::Dummy({10 * g_s, h});
}

bool banner(const char* text, float width) {
  const Palette& p = palette();
  const float h = ImGui::GetFrameHeight();
  const ImVec2 a = snap(ImGui::GetCursorScreenPos());
  const ImVec2 b = {a.x + std::floor(width), a.y + h};
  const bool clicked = ImGui::InvisibleButton("##banner", {width, h});
  ImDrawList* dl = ImGui::GetWindowDrawList();
  dl->AddRectFilled(a, b, p.well);
  draw_bevel(dl, a, b, Bevel::Sunken);
  dl->PushClipRect(a, b, true);
  dl->AddText({a.x + 8 * g_s, a.y + (h - ImGui::GetTextLineHeight()) * 0.5f}, p.well_text, text);
  dl->PopClipRect();
  return clicked;
}

bool begin_dialog(const char* title) {
  const ImGuiViewport* vp = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Appearing, {0.5f, 0.5f});
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  const bool open = ImGui::BeginPopupModal(
      title, nullptr,
      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize |
          ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
  ImGui::PopStyleVar();
  if (!open) return false;

  const Palette& p = palette();
  ImDrawList* dl = ImGui::GetWindowDrawList();
  const ImVec2 wmin = ImGui::GetWindowPos();
  const ImVec2 wmax = {wmin.x + ImGui::GetWindowWidth(), wmin.y + ImGui::GetWindowHeight()};
  draw_bevel(dl, wmin, wmax, Bevel::Raised);

  // Title bar: black strip with white caption, inset by the bevel.
  const char* end = label_end(title);
  const float bar_h = ImGui::GetTextLineHeight() + 6 * g_s;
  const ImVec2 bmin = {wmin.x + 3 * g_px, wmin.y + 3 * g_px};
  const ImVec2 bmax = {wmax.x - 3 * g_px, bmin.y + bar_h};
  dl->AddRectFilled(snap(bmin), snap(bmax), p.well);
  dl->AddText({bmin.x + 6 * g_s, bmin.y + 3 * g_s}, p.well_text, title, end);

  // Reserve the bar's height, and enough width for the caption.
  const ImVec2 cursor = ImGui::GetCursorScreenPos();
  ImGui::SetCursorScreenPos({cursor.x, bmax.y});
  ImGui::Dummy({ImGui::CalcTextSize(title, end).x + 48 * g_s, ImGui::GetStyle().ItemSpacing.y});
  return true;
}

void end_dialog() { ImGui::EndPopup(); }

bool begin_tooltip() {
  const Palette& p = palette();
  // Pushed in the parent window and popped there again in end_tooltip().
  ImGui::PushStyleColor(ImGuiCol_PopupBg, g4(p.field));
  ImGui::PushStyleColor(ImGuiCol_Border, g4(p.dark_shadow));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {8 * g_s, 6 * g_s});
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {8 * g_s, 3 * g_px});
  if (ImGui::BeginTooltip()) return true;
  ImGui::PopStyleVar(2);
  ImGui::PopStyleColor(2);
  return false;
}

void end_tooltip() {
  ImGui::EndTooltip();
  ImGui::PopStyleVar(2);
  ImGui::PopStyleColor(2);
}

void lcd_readout(const char* text, ImVec2 size) {
  push_timer_font();
  const ImVec2 t = ImGui::CalcTextSize(text);
  const float pad_x = 16 * g_s;
  const float pad_y = 4 * g_s;
  if (size.x <= 0) size.x = t.x + 2 * pad_x;
  if (size.y <= 0) size.y = t.y + 2 * pad_y;

  const Palette& p = palette();
  ImDrawList* dl = ImGui::GetWindowDrawList();
  const ImVec2 min = snap(ImGui::GetCursorScreenPos());
  const ImVec2 max = {min.x + std::floor(size.x), min.y + std::floor(size.y)};
  dl->AddRectFilled(min, max, p.well);
  draw_bevel(dl, min, max, Bevel::Sunken);
  dl->AddText(snap({min.x + (size.x - t.x) * 0.5f, min.y + (size.y - t.y) * 0.5f}), p.well_text,
              text);
  pop_font();
  ImGui::Dummy(size);
}

float status_bar_height() {
  return ImGui::GetTextLineHeight() + 6 * g_s + ImGui::GetStyle().WindowPadding.y;
}

void status_bar(std::initializer_list<const char*> cells) {
  const Palette& p = palette();
  ImDrawList* dl = ImGui::GetWindowDrawList();
  const ImVec2 wmin = ImGui::GetWindowPos();
  const ImVec2 wsize = ImGui::GetWindowSize();
  const float h = ImGui::GetTextLineHeight() + 6 * g_s;
  const float gap = 3 * g_s;
  const float y0 = std::floor(wmin.y + wsize.y - h - 3 * g_s);

  // The last cell stretches; the others fit their text.
  float x = wmin.x + gap;
  const float right = wmin.x + wsize.x - gap;
  std::size_t i = 0;
  for (const char* cell : cells) {
    const bool last = ++i == cells.size();
    const float w = last ? right - x : ImGui::CalcTextSize(cell).x + 12 * g_s;
    const ImVec2 a = snap({x, y0});
    const ImVec2 b = snap({x + w, y0 + h});
    draw_bevel(dl, a, b, Bevel::Status);
    dl->PushClipRect(a, b, true);
    dl->AddText({a.x + 6 * g_s, a.y + 3 * g_s}, p.text, cell);
    dl->PopClipRect();
    x += w + gap;
  }
}

void push_timer_font() { ImGui::PushFont(nullptr, kTimerFontSize); }
void push_small_font() { ImGui::PushFont(nullptr, kSmallFontSize); }
void pop_font() { ImGui::PopFont(); }

}  // namespace mattrack::ui
