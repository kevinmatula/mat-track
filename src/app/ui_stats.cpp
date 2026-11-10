// Header strip: level, rank, XP bar, current streak and today's grade, plus
// the new-record banner and the Settings button. All derived from the
// session log on the fly (every category, ignoring the History filter).
#include <imgui.h>

#include <string>

#include "app/app.hpp"
#include "app/theme.hpp"
#include "core/gamification.hpp"

namespace mattrack {

namespace {

// 12345 -> "12,345"
std::string with_commas(std::int64_t n) {
  std::string digits = std::to_string(n);
  for (int i = static_cast<int>(digits.size()) - 3; i > 0; i -= 3) digits.insert(i, ",");
  return digits;
}

}  // namespace

void App::draw_header(float width) {
  const float s = ui::scale();
  const float spacing = ImGui::GetStyle().ItemSpacing.x;
  const ImVec2 start = ImGui::GetCursorScreenPos();
  ui::begin_toolbar(width);

  const LevelProgress lp = level_progress(xp_for_seconds(records_.total_seconds));
  ImGui::AlignTextToFramePadding();
  ImGui::Text("LVL %d", lp.level);
  ImGui::SameLine();
  ui::progress_bar(static_cast<float>(lp.fraction()), {150 * s, ImGui::GetFrameHeight()});
  ImGui::SameLine();
  ImGui::Text("%s / %s XP", with_commas(lp.xp).c_str(), with_commas(lp.next_xp).c_str());

  ImGui::SameLine();
  ui::toolbar_separator();
  ImGui::SameLine();
  const Date t = today();
  const int streak = current_streak(all_days_, t, config_.streak_min_minutes);
  ImGui::Text("Streak %d day%s", streak, streak == 1 ? "" : "s");

  ImGui::SameLine();
  ui::toolbar_separator();
  ImGui::SameLine();
  const auto it = all_days_.find(t);
  const Grade grade =
      daily_grade(it == all_days_.end() ? 0 : it->second.seconds, goals_.on(t));
  ImGui::Text("Today's Grade: %s", std::string(grade_label(grade)).c_str());

  // Settings on the right; the banner (if any) fills the space before it.
  const float settings_w = 96 * s;
  const float right = start.x + width - 8 * s - settings_w;
  if (!banner_.empty()) {
    ImGui::SameLine();
    ui::toolbar_separator();
    ImGui::SameLine();
    const float w = right - spacing - ImGui::GetCursorScreenPos().x;
    if (w > 40 * s && ui::banner(banner_.c_str(), w)) banner_.clear();  // click to dismiss
  }
  ImGui::SameLine();
  ImGui::SetCursorScreenPos({right, ImGui::GetCursorScreenPos().y});
  if (ui::button("Settings...", {settings_w, 0})) open_settings_dialog_ = true;

  ui::end_toolbar();
}

}  // namespace mattrack
