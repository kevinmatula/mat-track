#include "app/theme.hpp"

#include <cstring>

#include "app/assets.hpp"

namespace mattrack::ui {

namespace {

bool g_inverted = false;

ImVec4 gray4(float level) {
  const float v = g_inverted ? 1.0f - level : level;
  return {v, v, v, 1.0f};
}

}  // namespace

ImU32 gray(float level) { return ImGui::GetColorU32(gray4(level)); }
ImU32 fg() { return gray(1.0f); }
ImU32 bg() { return gray(0.0f); }
bool inverted() { return g_inverted; }

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

  ImGuiStyle& style = ImGui::GetStyle();
  style = ImGuiStyle();

  style.WindowPadding = {24, 20};
  style.FramePadding = {10, 6};
  style.ItemSpacing = {12, 10};
  style.ItemInnerSpacing = {8, 6};
  style.ScrollbarSize = 10;
  style.GrabMinSize = 10;

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
  style.FrameBorderSize = 1;
  style.TabBorderSize = 1;

  style.FontSizeBase = kBaseFontSize;
  style.ScaleAllSizes(display_scale);
  style.FontScaleDpi = display_scale;

  ImVec4* c = style.Colors;
  for (int i = 0; i < ImGuiCol_COUNT; ++i) c[i] = gray4(0.0f);

  c[ImGuiCol_Text] = gray4(1.0f);
  c[ImGuiCol_TextDisabled] = gray4(0.45f);
  c[ImGuiCol_WindowBg] = gray4(0.0f);
  c[ImGuiCol_ChildBg] = gray4(0.0f);
  c[ImGuiCol_PopupBg] = gray4(0.0f);
  c[ImGuiCol_Border] = gray4(0.55f);
  c[ImGuiCol_BorderShadow] = {0, 0, 0, 0};

  c[ImGuiCol_FrameBg] = gray4(0.0f);
  c[ImGuiCol_FrameBgHovered] = gray4(0.15f);
  c[ImGuiCol_FrameBgActive] = gray4(0.25f);

  // Standard ImGui buttons/headers: hover is the brightest fill we can use
  // while keeping white text readable. ui::button() does true inversion.
  c[ImGuiCol_Button] = gray4(0.0f);
  c[ImGuiCol_ButtonHovered] = gray4(0.25f);
  c[ImGuiCol_ButtonActive] = gray4(0.4f);
  c[ImGuiCol_Header] = gray4(0.2f);
  c[ImGuiCol_HeaderHovered] = gray4(0.3f);
  c[ImGuiCol_HeaderActive] = gray4(0.4f);

  c[ImGuiCol_CheckMark] = gray4(1.0f);
  c[ImGuiCol_SliderGrab] = gray4(0.7f);
  c[ImGuiCol_SliderGrabActive] = gray4(1.0f);
  c[ImGuiCol_ScrollbarBg] = gray4(0.0f);
  c[ImGuiCol_ScrollbarGrab] = gray4(0.3f);
  c[ImGuiCol_ScrollbarGrabHovered] = gray4(0.5f);
  c[ImGuiCol_ScrollbarGrabActive] = gray4(0.7f);
  c[ImGuiCol_Separator] = gray4(0.35f);
  c[ImGuiCol_SeparatorHovered] = gray4(0.6f);
  c[ImGuiCol_SeparatorActive] = gray4(1.0f);
  c[ImGuiCol_TextSelectedBg] = gray4(0.35f);
  c[ImGuiCol_NavCursor] = gray4(1.0f);
  c[ImGuiCol_ModalWindowDimBg] = {0, 0, 0, 0.6f};
}

bool button(const char* label, ImVec2 size, bool enabled) {
  const char* label_end = std::strstr(label, "##");
  const ImVec2 text = ImGui::CalcTextSize(label, label_end);
  const ImGuiStyle& style = ImGui::GetStyle();
  if (size.x <= 0) size.x = text.x + style.FramePadding.x * 2;
  if (size.y <= 0) size.y = text.y + style.FramePadding.y * 2;

  const ImVec2 min = ImGui::GetCursorScreenPos();
  const ImVec2 max = {min.x + size.x, min.y + size.y};

  ImGui::BeginDisabled(!enabled);
  const bool pressed = ImGui::InvisibleButton(label, size);
  const bool hot = enabled && (ImGui::IsItemHovered() || ImGui::IsItemActive());
  ImGui::EndDisabled();

  ImDrawList* dl = ImGui::GetWindowDrawList();
  const ImU32 border = enabled ? fg() : gray(0.35f);
  dl->AddRectFilled(min, max, hot ? fg() : bg());
  dl->AddRect(min, max, border);
  const ImVec2 text_pos = {min.x + (size.x - text.x) * 0.5f, min.y + (size.y - text.y) * 0.5f};
  dl->AddText(text_pos, hot ? bg() : (enabled ? fg() : gray(0.45f)), label, label_end);
  return pressed && enabled;
}

void push_timer_font() { ImGui::PushFont(nullptr, kTimerFontSize); }
void pop_font() { ImGui::PopFont(); }

}  // namespace mattrack::ui
