// "Settings" dialog (Settings... button or F3): goals, timer lengths and
// alerts. OK saves config.toml and applies everything right away, except
// that a running session or break keeps its length.
#include <imgui.h>

#include <string>

#include "app/app.hpp"
#include "app/theme.hpp"
#include "core/config.hpp"

namespace mattrack {

namespace {

constexpr const char* kSettingsDialog = "Settings";

}  // namespace

void App::apply_settings(const Config& edited) {
  // One validation path for everything: the same clamping as config.toml.
  const Config c = parse_config(serialize_config(edited));
  try {
    save_config(c, data_dir_ / "config.toml");
  } catch (const std::exception& e) {
    status_ = std::string("Error: settings not saved (") + e.what() + ")";
    return;
  }
  // A new daily goal applies from today on; past days keep their grades.
  if (c.daily_goal_minutes != config_.daily_goal_minutes) {
    try {
      sync_goal(*db_, today(), c.daily_goal_minutes);
    } catch (const std::exception& e) {
      status_ = std::string("Error: goal not saved (") + e.what() + ")";
      return;
    }
  }
  config_ = c;
  pending_timer_settings_ = TimerSettings::from_config(config_);
  const bool applied_now = timer_.set_settings(*pending_timer_settings_);
  if (applied_now) pending_timer_settings_.reset();
  reload_history();  // goal, streak minimum and week start all affect it
  status_ = applied_now ? "Settings saved." : "Settings saved; timer changes start next session.";
}

void App::draw_settings_dialog() {
  if (open_settings_dialog_) {
    ImGui::OpenPopup(kSettingsDialog);
    open_settings_dialog_ = false;
    edit_config_ = config_;
  }
  if (!ui::begin_dialog(kSettingsDialog)) return;

  const float s = ui::px();
  const float group_w = 360 * s;
  const float field_w = 80 * s;
  // Label, number field, unit; the fields line up in one column (SameLine
  // offsets inside a group box count from its indented left edge).
  const float field_x = ImGui::CalcTextSize("Long break every ").x + 8 * s;
  auto row = [&](const char* label, const char* unit, auto&& field) {
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    ImGui::SameLine(field_x);
    field();
    ImGui::SameLine();
    ImGui::TextUnformatted(unit);
  };
  Config& c = edit_config_;

  ui::begin_group("Goals", group_w);
  row("Daily goal", "minutes",
      [&] { ui::input_int("##goal", &c.daily_goal_minutes, 1, 1440, field_w); });
  row("Streak minimum", "minutes a day",
      [&] { ui::input_int("##streak", &c.streak_min_minutes, 1, 1440, field_w); });
  ui::end_group();

  ui::begin_group("Timer", group_w);
  row("Focus", "minutes",
      [&] { ui::input_minutes("##focus", &c.focus_minutes, 0.1, 600, field_w); });
  row("Short break", "minutes",
      [&] { ui::input_minutes("##break", &c.break_minutes, 0.1, 600, field_w); });
  row("Long break", "minutes",
      [&] { ui::input_minutes("##long", &c.long_break_minutes, 0.1, 600, field_w); });
  row("Long break every", "sessions",
      [&] { ui::input_int("##every", &c.long_break_every, 1, 100, field_w); });
  ui::end_group();

  ui::begin_group("Alerts", group_w);
  ui::checkbox("Play a jingle when time is up", &c.sound_enabled);
  ui::checkbox("Bring the window to the front", &c.raise_on_alarm);
  ui::end_group();

  const bool busy = timer_.state() != TimerState::Idle;
  if (busy) ImGui::TextDisabled("Timer changes start with the next session.");
  ImGui::Spacing();
  if (ui::button("OK", {}, true, true)) {
    apply_settings(edit_config_);
    ImGui::CloseCurrentPopup();
  }
  ImGui::SameLine();
  if (ui::button("Cancel")) ImGui::CloseCurrentPopup();

  ui::end_dialog();
}

}  // namespace mattrack
