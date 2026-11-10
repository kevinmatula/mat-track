// Day panel: the sessions of the day selected in the History panel, with an
// "Edit session" dialog to change a session's category or note, or delete it.
#include <imgui.h>

#include <cmath>
#include <cstring>
#include <string>

#include "app/app.hpp"
#include "app/theme.hpp"
#include "core/gamification.hpp"
#include "core/time_util.hpp"

namespace mattrack {

namespace {

constexpr const char* kSessionDialog = "Edit session";
constexpr std::array<const char*, 7> kWeekdays = {"Sunday",   "Monday", "Tuesday", "Wednesday",
                                                  "Thursday", "Friday", "Saturday"};

std::string weekday_and_date(Date d) {
  const std::chrono::weekday wd{std::chrono::sys_days{d}};
  return std::string(kWeekdays[wd.c_encoding()]) + " " + format_date(d);
}

// `text` shortened with "..." to fit `width`.
std::string fit_text(const std::string& text, float width) {
  if (ImGui::CalcTextSize(text.c_str()).x <= width) return text;
  std::string out = text;
  while (!out.empty() && ImGui::CalcTextSize((out + "...").c_str()).x > width) out.pop_back();
  return out + "...";
}

}  // namespace

void App::draw_day_panel(float width, float height) {
  const float s = ui::px();
  const float bottom = ImGui::GetCursorScreenPos().y + height;
  ui::begin_group(selected_day_ ? "Day" : "Overview", width, height);

  if (!selected_day_) {
    draw_overview();
    ui::end_group();
    return;
  }

  const Date day = *selected_day_;
  const std::vector<Session> sessions = sessions_on(day);
  std::int64_t total = 0;
  for (const auto& x : sessions) total += x.duration_sec;

  ImGui::TextUnformatted(weekday_and_date(day).c_str());
  if (filter_) {
    ImGui::SameLine();
    ImGui::TextDisabled("(%s only)", category_name(*filter_));
  }
  if (sessions.empty()) {
    ImGui::TextDisabled("No study");
  } else {
    ImGui::Text("%s   %zu session%s   grade %s", format_duration(total).c_str(), sessions.size(),
                sessions.size() == 1 ? "" : "s",
                std::string(grade_label(daily_grade(total, goals_.on(day)))).c_str());
  }

  // The list fills the rest of the panel (which matches the timer's height).
  const float list_h = std::max(bottom - ImGui::GetStyle().WindowPadding.y -
                                    ImGui::GetStyle().ItemSpacing.y -
                                    ImGui::GetCursorScreenPos().y,
                                4 * ImGui::GetFrameHeightWithSpacing());
  ui::begin_list("##sessions",
                 {{"Start", 64 * s},
                  {"Time", 76 * s},
                  {"Category", 120 * s},
                  {"Completed", ImGui::CalcTextSize("Completed").x + 16 * s},
                  {"Note", 0},
                  {"", 88 * s}},
                 {ui::avail_width(), list_h});
  const float inset = 6 * s;
  if (sessions.empty()) {
    ImGui::SetCursorPosX(inset);
    ImGui::TextDisabled("No sessions.");
  }
  for (const auto& x : sessions) {
    ImGui::PushID(static_cast<int>(x.id));
    ImGui::SetCursorPosX(inset);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(format_clock(x.start_utc, *tz_).c_str());
    ImGui::SameLine(ui::list_column_x(1) + inset);
    ImGui::TextUnformatted(format_duration(x.duration_sec).c_str());
    ImGui::SameLine(ui::list_column_x(2) + inset);
    ImGui::TextUnformatted(fit_text(category_name(x.category_id),
                                    ui::list_column_width(2) - 2 * inset)
                               .c_str());
    // Checked if the session ran to its full length, not stopped early.
    const float box = std::floor(ImGui::GetTextLineHeight() * 0.8f);  // ui::check_mark's size
    ImGui::SameLine(ui::list_column_x(3) + std::floor((ui::list_column_width(3) - box) * 0.5f));
    ui::check_mark(x.completed);
    ImGui::SameLine(ui::list_column_x(4) + inset);
    ImGui::TextUnformatted(fit_text(x.note, ui::list_column_width(4) - 2 * inset).c_str());
    ImGui::SameLine(ui::list_column_x(5) + inset);
    if (ui::button("Edit...", {ui::list_column_width(5) - 2 * inset, 0})) {
      editing_ = x;
      edit_category_ = x.category_id;
      edit_note_.fill('\0');
      std::strncpy(edit_note_.data(), x.note.c_str(), edit_note_.size() - 1);
      confirm_delete_ = false;
      open_session_dialog_ = true;  // opened from the main window, not the list
    }
    ImGui::PopID();
  }
  ui::end_list();
  ui::end_group();
}

// Shown in the day panel when no day is selected: this week and the
// all-time records. Every category, ignoring the filter. Two columns when
// there is room; otherwise one compact column without the category split.
void App::draw_overview() {
  const float s = ui::px();
  const float avail = ui::avail_width();
  const bool two_columns = avail >= 560 * s;
  const float x0 = ImGui::GetCursorPosX();
  const float y0 = ImGui::GetCursorPosY();
  // "Oct 18, 2026" -> "Oct 18"
  auto month_day = [](Date d) {
    const std::string full = format_short_date(d);
    return full.substr(0, full.find(','));
  };
  // Label in gray, value at `value_x` (SameLine offsets count from the
  // enclosing group's left edge).
  auto pair = [](float value_x, const char* label, const std::string& value) {
    ImGui::TextDisabled("%s", label);
    ImGui::SameLine(value_x);
    ImGui::TextUnformatted(value.c_str());
  };

  // This week.
  const Date t = today();
  const DateSpan week = week_of(t, config_.week_starts_on);
  const DayTotal wk = total_between(all_days_, week.first, week.last);
  const std::string grade = std::string(
      grade_label(weekly_grade(all_days_, t, config_.week_starts_on, goals_)));
  const std::string range = month_day(week.first) + " - " + month_day(week.last);
  const float rvx = ImGui::CalcTextSize("Longest session  ").x;  // records' value column

  if (two_columns) {
    ImGui::Text("This week, %s", range.c_str());
    const float wx = ImGui::CalcTextSize("Physics   ").x + 24 * s;
    pair(wx, "Total", format_duration(wk.seconds));
    pair(wx, "Grade", grade);
    // Per-category split, most time first (room for a few).
    int shown = 0;
    for (const auto& c : wk.by_category) {
      if (++shown > 4) break;
      ImGui::TextDisabled("%s", fit_text(category_name(c.category_id), wx - 8 * s).c_str());
      ImGui::SameLine(wx);
      ImGui::TextUnformatted(format_duration(c.seconds).c_str());
    }
    if (wk.by_category.empty()) ImGui::TextDisabled("Nothing yet this week.");
    // The records column has the longer lines, so it gets more room.
    ImGui::SetCursorPos({x0 + std::floor(avail * 0.42f), y0});
  } else {
    pair(rvx, "This week", format_duration(wk.seconds) + ", grade " + grade);
    ImGui::Spacing();
  }

  ImGui::BeginGroup();
  ImGui::TextUnformatted("Records");
  const Records& r = records_;
  // Drop the year when space is tight.
  auto when = [&](Date d) { return two_columns ? format_short_date(d) : month_day(d); };
  pair(rvx, "Best day",
       r.best_day ? format_duration(r.best_day->seconds) + " on " + when(r.best_day->day) : "-");
  pair(rvx, "Longest session",
       r.longest_session
           ? format_duration(r.longest_session->seconds) + " on " + when(r.longest_session->day)
           : "-");
  pair(rvx, "Longest streak",
       std::to_string(r.longest_streak) + (r.longest_streak == 1 ? " day" : " days"));
  pair(rvx, "Total",
       format_duration(r.total_seconds) + " in " + std::to_string(r.total_sessions) +
           (r.total_sessions == 1 ? " session" : " sessions"));
  ImGui::EndGroup();

  if (two_columns) {
    ImGui::Spacing();
    ImGui::SetCursorPosX(x0);
    ImGui::TextDisabled("Click a day in History to see its sessions.");
  }
}

void App::draw_session_dialog() {
  if (open_session_dialog_) {
    ImGui::OpenPopup(kSessionDialog);
    open_session_dialog_ = false;
  }
  if (!editing_ || !ui::begin_dialog(kSessionDialog)) return;

  const float s = ui::px();
  const Session& x = *editing_;
  ImGui::Text("%s, %s, %s", weekday_and_date(day_key(x.start_utc, *tz_, config_.day_rollover_hour))
                                .c_str(),
              format_clock(x.start_utc, *tz_).c_str(), format_duration(x.duration_sec).c_str());
  ImGui::TextDisabled("%s", x.completed ? "Completed" : "Stopped early");
  ImGui::Spacing();

  auto close = [&] {
    ImGui::CloseCurrentPopup();
    editing_.reset();
  };

  if (confirm_delete_) {
    ImGui::TextUnformatted("Delete this session? This cannot be undone.");
    ImGui::Spacing();
    if (ui::button("Delete", {}, true, true)) {
      try {
        db_->delete_session(x.id);
        if (note_session_id_ == x.id) note_session_id_.reset();
        reload_history();
        status_ = "Session deleted.";
      } catch (const std::exception& e) {
        status_ = std::string("Error: session not deleted (") + e.what() + ")";
      }
      close();
    }
    ImGui::SameLine();
    if (ui::button("Cancel")) confirm_delete_ = false;
    ui::end_dialog();
    return;
  }

  const float label_w = ImGui::CalcTextSize("Category").x + 12 * s;
  const float field_w = 300 * s;

  // Category: any active one, plus the session's own if it is archived.
  ImGui::AlignTextToFramePadding();
  ImGui::TextUnformatted("Category");
  ImGui::SameLine(label_w + ImGui::GetStyle().WindowPadding.x);
  ImGui::SetNextItemWidth(field_w);
  if (ui::begin_combo("##edit_category", category_name(edit_category_))) {
    for (const auto& c : all_categories_) {
      if (c.archived && c.id != x.category_id) continue;
      if (ImGui::Selectable(c.name.c_str(), c.id == edit_category_)) edit_category_ = c.id;
    }
    ui::end_combo();
  }

  ImGui::AlignTextToFramePadding();
  ImGui::TextUnformatted("Note");
  ImGui::SameLine(label_w + ImGui::GetStyle().WindowPadding.x);
  const bool enter = ui::input_text("##edit_note", edit_note_.data(), edit_note_.size(), field_w);

  ImGui::Spacing();
  if (ui::button("OK", {}, true, true) || enter) {
    try {
      if (edit_category_ != x.category_id) db_->update_session_category(x.id, edit_category_);
      if (x.note != edit_note_.data()) db_->update_session_note(x.id, edit_note_.data());
      reload_history();
      status_ = "Session updated.";
    } catch (const std::exception& e) {
      status_ = std::string("Error: session not updated (") + e.what() + ")";
    }
    close();
  }
  ImGui::SameLine();
  if (ui::button("Cancel")) close();
  // Delete sits apart, on the right.
  ImGui::SameLine(label_w + ImGui::GetStyle().WindowPadding.x + field_w - 75 * s);
  if (ui::button("Delete...")) confirm_delete_ = true;

  ui::end_dialog();
}

}  // namespace mattrack
