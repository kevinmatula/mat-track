// Timer panel: countdown, category, controls, post-session note, and the
// interrupted-session dialog.
#include <imgui.h>

#include <cstdio>

#include "app/app.hpp"
#include "app/theme.hpp"
#include "core/time_util.hpp"

namespace mattrack {

namespace {

constexpr float kPanelWidth = 440.0f;

void format_mmss(std::int64_t seconds, char* out, std::size_t size) {
  std::snprintf(out, size, "%02lld:%02lld", static_cast<long long>(seconds / 60),
                static_cast<long long>(seconds % 60));
}

}  // namespace

void App::draw_timer_panel() {
  const TimerState st = timer_.state();
  const bool active = st == TimerState::Running || st == TimerState::Paused;

  ui::begin_group("Timer", kPanelWidth * ui_scale_);

  char digits[16];
  const std::int64_t shown =
      countdown_seconds() >= 0 ? countdown_seconds() : timer_.focus_target() / kNsPerSecond;
  format_mmss(shown, digits, sizeof digits);
  ui::lcd_readout(digits, {ui::avail_width(), 0});

  // One line saying what the timer is doing.
  const int every = timer_.long_break_every();
  char line[96];
  switch (st) {
    case TimerState::Running:
      std::snprintf(line, sizeof line, "FOCUS   pomodoro %d of %d",
                    timer_.completed_in_cycle() % every + 1, every);
      break;
    case TimerState::Paused:
      std::snprintf(line, sizeof line, "PAUSED  pomodoro %d of %d",
                    timer_.completed_in_cycle() % every + 1, every);
      break;
    case TimerState::BreakReady:
      std::snprintf(line, sizeof line, "DONE    time for a %s",
                    timer_.on_long_break() ? "long break" : "break");
      break;
    case TimerState::Break:
      std::snprintf(line, sizeof line, "%s", timer_.on_long_break() ? "LONG BREAK" : "BREAK");
      break;
    default:
      if (alarm_ == Alarm::BreakDone) {
        std::snprintf(line, sizeof line, "BREAK OVER");
      } else {
        std::snprintf(line, sizeof line, "READY   %d of %d done this cycle",
                      timer_.completed_in_cycle(), every);
      }
      break;
  }
  ImGui::TextUnformatted(line);

  // Category: fixed once a session has started.
  const char* preview = "";
  for (const auto& c : categories_) {
    if (c.id == (active ? timer_.category_id() : selected_category_)) preview = c.name.c_str();
  }
  ImGui::AlignTextToFramePadding();
  ImGui::TextUnformatted("Category");
  ImGui::SameLine();
  const float edit_w = 75.0f * ui::px();
  ImGui::SetNextItemWidth(ui::avail_width() - edit_w - ImGui::GetStyle().ItemSpacing.x);
  if (ui::begin_combo("##category", preview, !active)) {
    for (const auto& c : categories_) {
      const bool selected = c.id == selected_category_;
      if (ImGui::Selectable(c.name.c_str(), selected)) selected_category_ = c.id;
      if (selected) ImGui::SetItemDefaultFocus();
    }
    ui::end_combo();
  }
  ImGui::SameLine();
  if (ui::button("Edit...##categories", {edit_w, 0})) open_categories_dialog_ = true;

  ImGui::Spacing();
  switch (st) {
    case TimerState::Running:
      if (ui::button("Pause", {}, true, true)) pause_session();
      ImGui::SameLine();
      if (ui::button("Stop")) stop_session();
      break;
    case TimerState::Paused:
      if (ui::button("Resume", {}, true, true)) resume_session();
      ImGui::SameLine();
      if (ui::button("Stop")) stop_session();
      break;
    case TimerState::BreakReady:
      if (ui::button("Start break", {}, true, true)) start_break();
      ImGui::SameLine();
      if (ui::button("Skip break")) skip_break();
      break;
    case TimerState::Break:
      if (ui::button("Skip break", {}, true, true)) skip_break();
      break;
    default:
      if (ui::button("Start", {}, !categories_.empty(), true)) start_session();
      ImGui::SameLine();
      if (alarm_ == Alarm::BreakDone) {
        if (ui::button("Dismiss")) silence();
      } else {
        ui::button("Stop", {}, false);
      }
      break;
  }

  if (active && timer_.suspend_detected()) {
    ImGui::Spacing();
    ImGui::TextWrapped("Suspend detected; session time not counted for the gap.");
  }
  draw_note_row();

  ui::end_group();
}

// One row at the bottom of the timer panel after a session is logged.
void App::draw_note_row() {
  if (!note_session_id_) return;

  ImGui::Spacing();
  ImGui::AlignTextToFramePadding();
  ImGui::TextUnformatted("Note");
  ImGui::SameLine();
  const float buttons = 2 * 75.0f * ui::px() + 2 * ImGui::GetStyle().ItemSpacing.x;
  bool save = ui::input_text("##note", note_buf_.data(), note_buf_.size(),
                             ui::avail_width() - buttons);
  ImGui::SameLine();
  save |= ui::button("Save", {}, true, true);
  ImGui::SameLine();
  const bool skip = ui::button("Skip");

  if (save) {
    try {
      db_->update_session_note(*note_session_id_, note_buf_.data());
      reload_history();  // the day panel lists notes from the cache
      status_ = "Note saved.";
    } catch (const std::exception& e) {
      status_ = std::string("Error: note not saved (") + e.what() + ")";
    }
    note_session_id_.reset();
  } else if (skip) {
    note_session_id_.reset();
  }
}

void App::draw_recovery_dialog() {
  constexpr const char* kTitle = "Interrupted session";
  if (!pending_recovery_) return;
  if (!ImGui::IsPopupOpen(kTitle)) ImGui::OpenPopup(kTitle);
  if (!ui::begin_dialog(kTitle)) return;

  ImGui::Text("A session was interrupted (%s).",
              format_duration(pending_recovery_->focused_seconds).c_str());
  ImGui::TextUnformatted("Save it?");
  ImGui::Spacing();
  if (ui::button("Save", {}, true, true)) {
    save_recovered_session();
    ImGui::CloseCurrentPopup();
  }
  ImGui::SameLine();
  if (ui::button("Discard")) {
    discard_recovered_session();
    ImGui::CloseCurrentPopup();
  }
  ui::end_dialog();
}

}  // namespace mattrack
