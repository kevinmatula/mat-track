// History panel: the year heatmap, plus month and week views that use the
// same shaded cells, larger. All in one black well, with a tooltip per day
// and the period's total and daily average underneath.
#include <imgui.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <optional>
#include <string>

#include "app/app.hpp"
#include "app/theme.hpp"
#include "core/heatmap.hpp"
#include "core/time_util.hpp"

namespace mattrack {

namespace {

namespace chr = std::chrono;

// The well is black in both palettes, so these grays from the spec are fixed.
constexpr std::array<ImU32, kHeatmapLevels> kLevelColors = {
    IM_COL32(0x11, 0x11, 0x11, 255),  // L0: nothing
    IM_COL32(0x3A, 0x3A, 0x3A, 255),  // L1: under 25% of the goal
    IM_COL32(0x6E, 0x6E, 0x6E, 255),  // L2: 25-50%
    IM_COL32(0xB0, 0xB0, 0xB0, 255),  // L3: 50-100%
    IM_COL32(0xFF, 0xFF, 0xFF, 255),  // L4: goal met
};
constexpr ImU32 kEmptyOutline = IM_COL32(0x2E, 0x2E, 0x2E, 255);
constexpr ImU32 kLabel = IM_COL32(0x9A, 0x9A, 0x9A, 255);
constexpr ImU32 kDim = IM_COL32(0x55, 0x55, 0x55, 255);
constexpr ImU32 kBright = IM_COL32(0xFF, 0xFF, 0xFF, 255);

constexpr std::array<const char*, 12> kMonths = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                                 "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
constexpr std::array<const char*, 12> kMonthNames = {
    "January", "February", "March",     "April",   "May",      "June",
    "July",    "August",   "September", "October", "November", "December"};
constexpr std::array<const char*, 7> kWeekdaysShort = {"Sun", "Mon", "Tue", "Wed",
                                                       "Thu", "Fri", "Sat"};

// Year-view cell size when the panel is at full width.
constexpr float kMaxYearCell = 16;
// Largest cell for the week and month rows.
constexpr float kMaxRowCell = 72;

ImU32 level_color(int level) { return kLevelColors[static_cast<std::size_t>(level)]; }

std::string short_date(Date d) {
  return std::string(kMonths[static_cast<unsigned>(d.month()) - 1]) + " " +
         std::to_string(static_cast<unsigned>(d.day()));
}

std::string month_title(Date d) {
  return std::string(kMonthNames[static_cast<unsigned>(d.month()) - 1]) + " " +
         std::to_string(static_cast<int>(d.year()));
}

void draw_cell(ImDrawList* dl, ImVec2 a, ImVec2 b, int level) {
  dl->AddRectFilled(a, b, level_color(level));
  if (level == 0) dl->AddRect(a, b, kEmptyOutline);
}

// A ring just outside a cell, plus an inner black ring so it still reads on
// a white (goal met) cell.
// The selected day (shown in the day panel): a thicker ring filling the gap
// around the cell, clearly different from today's thin one.
void draw_selected_ring(ImDrawList* dl, ImVec2 a, ImVec2 b, float s) {
  dl->AddRect({a.x - 2 * s, a.y - 2 * s}, {b.x + 2 * s, b.y + 2 * s}, kBright, 0.0f, 2 * s);
  // A black edge just inside, so the ring reads on a white cell too.
  dl->AddRect(a, b, IM_COL32_BLACK, 0.0f, s);
}

void draw_today_ring(ImDrawList* dl, ImVec2 a, ImVec2 b, int level, float s) {
  dl->AddRect({a.x - s, a.y - s}, {b.x + s, b.y + s}, kBright, 0.0f, s);
  if (level == 4) dl->AddRect({a.x + s, a.y + s}, {b.x - s, b.y - s}, IM_COL32_BLACK, 0.0f, s);
}

// What every view needs to draw into the well.
struct Well {
  ImDrawList* dl;
  ImVec2 min;  // content area (inside the padding)
  ImVec2 max;
  float s;
  float line_h;
  const DayTotals& days;
  const GoalHistory& goals;  // each day is shaded against its own goal
  Date today;
  WeekStart starts_on;
  bool can_hover;
  std::optional<Date>& hovered;
  std::optional<Date> selected;

  std::int64_t seconds(Date d) const {
    const auto it = days.find(d);
    return it == days.end() ? 0 : it->second.seconds;
  }
  int level(Date d) const { return intensity_level(seconds(d), goals.on(d)); }
  void hover(Date d, ImVec2 a, ImVec2 b) const {
    if (can_hover && d <= today && ImGui::IsMouseHoveringRect(a, b)) hovered = d;
  }
};

// Height every view gets (the year's), so switching views doesn't move the
// layout; the week and month rows fit easily inside it.
float content_height(float s, float line_h) {
  const float gap = 2 * s;
  return line_h + 6 * s + 7 * (kMaxYearCell * s + gap) - gap;
}

// Mon / Wed / Fri down the left of a weeks-as-columns grid.
void draw_weekday_labels(const Well& w, const HeatmapLayout& layout, float x, float grid_y,
                         float cell, float step) {
  for (const auto& [wd, name] : {std::pair{chr::Monday, "Mon"}, std::pair{chr::Wednesday, "Wed"},
                                 std::pair{chr::Friday, "Fri"}}) {
    const float y = grid_y + layout.row_of(wd) * step + (cell - w.line_h) * 0.5f;
    w.dl->AddText({x, std::floor(y)}, kLabel, name);
  }
}

void draw_year(const Well& w) {
  const float s = w.s;
  const HeatmapLayout layout = heatmap_layout(w.today, w.starts_on);
  const float gap = 2 * s;
  const float label_w = ImGui::CalcTextSize("Wed").x + 8 * s;
  const float avail = w.max.x - w.min.x;
  const float fit = (avail - label_w - (layout.weeks - 1) * gap) / layout.weeks;
  const float cell = std::clamp(std::floor(fit), 6 * s, kMaxYearCell * s);
  const float step = cell + gap;
  const float grid_w = layout.weeks * step - gap;

  // Center the labelled grid horizontally.
  const float gx = std::floor(w.min.x + (avail - label_w - grid_w) * 0.5f + label_w);
  const float gy = std::floor(w.min.y + w.line_h + 6 * s);

  // Month labels above the column holding each month's 1st, skipping any
  // that would overlap the previous one.
  float last_label_end = -1e9f;
  for (const MonthStart& m : month_starts(layout)) {
    const float x = gx + m.column * step;
    const char* name = kMonths[static_cast<unsigned>(m.month) - 1];
    if (x < last_label_end + 4 * s) continue;
    w.dl->AddText({x, w.min.y}, kLabel, name);
    last_label_end = x + ImGui::CalcTextSize(name).x;
  }

  draw_weekday_labels(w, layout, gx - label_w, gy, cell, step);

  // Cells. Future days are left empty and have no tooltip.
  for (int col = 0; col < layout.weeks; ++col) {
    for (int row = 0; row < 7; ++row) {
      const Date d = layout.day_at(col, row);
      if (d > w.today) continue;
      const ImVec2 a = {gx + col * step, gy + row * step};
      const ImVec2 b = {a.x + cell, a.y + cell};
      const int level = w.level(d);
      draw_cell(w.dl, a, b, level);
      if (d == w.today) draw_today_ring(w.dl, a, b, level, s);
      if (d == w.selected) draw_selected_ring(w.dl, a, b, s);
      w.hover(d, a, b);
    }
  }
}

// Future days in the month and week views: a faint outline, so the shape of
// the period stays readable (the year view simply leaves them out).
void draw_future_cell(ImDrawList* dl, ImVec2 a, ImVec2 b) { dl->AddRect(a, b, kEmptyOutline); }

// One horizontal row of day cells with a label above each, centered in the
// well: the week ("Mon 2") and the month ("1".."31") views.
template <typename LabelFn>
void draw_day_row(const Well& w, Date first, int count, float gap, LabelFn label_of,
                  bool small_labels) {
  const float s = w.s;
  if (small_labels) ui::push_small_font();
  const float label_h = ImGui::GetTextLineHeight() + 6 * s;
  const float avail_w = w.max.x - w.min.x;
  const float cell = std::floor(std::min({kMaxRowCell * s, (avail_w - (count - 1) * gap) / count,
                                          w.max.y - w.min.y - label_h}));
  const float step = cell + gap;
  const float row_w = count * step - gap;
  const float x0 = std::floor(w.min.x + (avail_w - row_w) * 0.5f);
  const float y0 = std::floor(w.min.y + (w.max.y - w.min.y - label_h - cell) * 0.5f);

  for (int i = 0; i < count; ++i) {
    const Date d = add_days(first, i);
    const float x = x0 + i * step;

    // Label above the cell: today in white, the future dimmed.
    const std::string label = label_of(d);
    const ImU32 color = d == w.today ? kBright : (d > w.today ? kDim : kLabel);
    w.dl->AddText({std::floor(x + (cell - ImGui::CalcTextSize(label.c_str()).x) * 0.5f), y0},
                  color, label.c_str());

    const ImVec2 a = {x, y0 + label_h};
    const ImVec2 b = {a.x + cell, a.y + cell};
    if (d > w.today) {
      draw_future_cell(w.dl, a, b);
      continue;
    }
    const int level = w.level(d);
    draw_cell(w.dl, a, b, level);
    if (d == w.today) draw_today_ring(w.dl, a, b, level, s);
    if (d == w.selected) draw_selected_ring(w.dl, a, b, s);
    w.hover(d, a, b);
  }
  if (small_labels) ui::pop_font();
}

// Three stacked rows of larger cells: days 1-10, 11-20 and 21-end, each
// labelled on the left like the year's weekday labels.
void draw_month(const Well& w, DateSpan month) {
  const float s = w.s;
  const int days =
      static_cast<int>((chr::sys_days{month.last} - chr::sys_days{month.first}).count()) + 1;
  constexpr int kRows = 3;
  constexpr int kPerRow = 10;
  // The last row takes the rest (8-11 days). Size and center for the
  // longest possible row so the block stays put from month to month.
  constexpr int kWidest = 11;

  const float gap = 4 * s;
  const float label_w = ImGui::CalcTextSize("21-31").x + 12 * s;
  const float avail_w = w.max.x - w.min.x;
  // Leave breathing room above and below so the rows don't crowd the
  // well's border (the year view has its month labels there).
  const float margin_y = 10 * s;
  const float cell = std::floor(
      std::min({kMaxRowCell * s, (w.max.y - w.min.y - 2 * margin_y - (kRows - 1) * gap) / kRows,
                (avail_w - label_w - (kWidest - 1) * gap) / kWidest}));
  const float step = cell + gap;
  const float block_w = label_w + kWidest * step - gap;
  const float block_h = kRows * step - gap;
  const float x0 = std::floor(w.min.x + (avail_w - block_w) * 0.5f + label_w);
  const float y0 = std::floor(w.min.y + (w.max.y - w.min.y - block_h) * 0.5f);

  for (int row = 0; row < kRows; ++row) {
    const int first = row * kPerRow + 1;
    const int last = row == kRows - 1 ? days : first + kPerRow - 1;
    const std::string label = std::to_string(first) + "-" + std::to_string(last);
    const float y = y0 + row * step;
    w.dl->AddText({x0 - label_w, std::floor(y + (cell - w.line_h) * 0.5f)}, kLabel,
                  label.c_str());

    for (int day = first; day <= last; ++day) {
      const Date d = add_days(month.first, day - 1);
      const ImVec2 a = {x0 + (day - first) * step, y};
      const ImVec2 b = {a.x + cell, a.y + cell};
      if (d > w.today) {
        draw_future_cell(w.dl, a, b);
        continue;
      }
      const int level = w.level(d);
      draw_cell(w.dl, a, b, level);
      if (d == w.today) draw_today_ring(w.dl, a, b, level, s);
      if (d == w.selected) draw_selected_ring(w.dl, a, b, s);
      w.hover(d, a, b);
    }
  }
}

void draw_week(const Well& w, DateSpan week) {
  draw_day_row(
      w, week.first, 7, 4 * w.s,
      [](Date d) {
        const chr::weekday wd{chr::sys_days{d}};
        return std::string(kWeekdaysShort[wd.c_encoding()]) + " " +
               std::to_string(static_cast<unsigned>(d.day()));
      },
      /*small_labels=*/false);
}

// "Total  6h 15m    Daily avg  1h 33m" at the bottom left of the well.
void draw_totals(ImDrawList* dl, ImVec2 pos, const PeriodSummary& p, float s) {
  float x = pos.x;
  auto pair = [&](const char* label, const std::string& value) {
    dl->AddText({x, pos.y}, kLabel, label);
    x += ImGui::CalcTextSize(label).x + 8 * s;
    dl->AddText({x, pos.y}, kBright, value.c_str());
    x += ImGui::CalcTextSize(value.c_str()).x + 24 * s;
  };
  pair("Total", format_duration(p.total_seconds));
  pair("Daily avg", format_duration(p.average_seconds()));
}

void draw_legend(ImDrawList* dl, float right, float y, float line_h, float s) {
  const float lc = std::floor(line_h * 0.7f);
  const float gap = 2 * s;
  const float less_w = ImGui::CalcTextSize("Less").x;
  const float total = less_w + 6 * s + kHeatmapLevels * (lc + gap) + 4 * s +
                      ImGui::CalcTextSize("More").x;
  float x = right - total;
  dl->AddText({x, y}, kLabel, "Less");
  x += less_w + 6 * s;
  const float cy = std::floor(y + (line_h - lc) * 0.5f);
  for (int level = 0; level < kHeatmapLevels; ++level) {
    draw_cell(dl, {x, cy}, {x + lc, cy + lc}, level);
    x += lc + gap;
  }
  dl->AddText({x + 4 * s, y}, kLabel, "More");
}

}  // namespace

void App::draw_history_toolbar() {
  const float s = ui::px();
  const std::pair<HistoryView, const char*> views[] = {
      {HistoryView::Year, "Year"}, {HistoryView::Month, "Month"}, {HistoryView::Week, "Week"}};
  const float toggle_w = ImGui::CalcTextSize("Month").x + 2 * ImGui::GetStyle().FramePadding.x;
  bool first = true;
  for (const auto& [view, name] : views) {
    if (!first) ImGui::SameLine(0, 0);  // touching, like a toolbar group
    first = false;
    if (ui::toggle_button(name, history_view_ == view, {toggle_w, 0}) && history_view_ != view) {
      history_view_ = view;
      history_back_ = 0;
    }
  }

  // Paging arrows around the period's name (fixed width so they don't jump).
  ImGui::SameLine(0, 16 * s);
  const float h = ImGui::GetFrameHeight();
  const bool paged = history_view_ != HistoryView::Year;
  if (ui::button("<##back", {h + 8 * s, h}, paged)) ++history_back_;
  ImGui::SameLine();

  const Date t = today();
  std::string label = "Past 12 months";
  if (history_view_ == HistoryView::Month) {
    label = month_title(month_back(t, history_back_).first);
  } else if (history_view_ == HistoryView::Week) {
    const DateSpan wk = week_back(t, config_.week_starts_on, history_back_);
    label = short_date(wk.first) + " - " + short_date(wk.last);
  }
  const float label_w = ImGui::CalcTextSize("September 2026").x + 16 * s;
  const ImVec2 pos = ImGui::GetCursorScreenPos();
  ImGui::GetWindowDrawList()->AddText(
      {std::floor(pos.x + (label_w - ImGui::CalcTextSize(label.c_str()).x) * 0.5f),
       std::floor(pos.y + (h - ImGui::GetTextLineHeight()) * 0.5f)},
      ui::palette().text, label.c_str());
  ImGui::Dummy({label_w, h});
  ImGui::SameLine();
  if (ui::button(">##fwd", {h + 8 * s, h}, paged && history_back_ > 0)) --history_back_;

  // Category filter, at the right end of the toolbar.
  const float combo_w = 180 * s;
  const float filter_label_w =
      ImGui::CalcTextSize("Category").x + ImGui::GetStyle().ItemSpacing.x;
  ImGui::SameLine();
  ImGui::SetCursorPosX(ImGui::GetCursorPosX() +
                       std::max(0.0f, ui::avail_width() - filter_label_w - combo_w));
  ImGui::AlignTextToFramePadding();
  ImGui::TextUnformatted("Category");
  ImGui::SameLine();
  ImGui::SetNextItemWidth(combo_w);
  const std::string preview =
      filter_ ? std::string(category_name(*filter_)) : std::string("All");
  if (ui::begin_combo("##filter", preview.c_str())) {
    if (ImGui::Selectable("All", !filter_)) {
      filter_.reset();
      apply_filter();
    }
    for (const auto& c : all_categories_) {
      const std::string name = c.archived ? c.name + " (archived)" : c.name;
      if (ImGui::Selectable(name.c_str(), filter_ == c.id)) {
        filter_ = c.id;
        apply_filter();
      }
    }
    ui::end_combo();
  }
}

void App::draw_heatmap_panel() {
  const float s = ui::px();
  const float pad = 12 * s;
  const float line_h = ImGui::GetTextLineHeight();

  // The panel grows with the window only up to its width in the default-size
  // window (where it fills edge to edge), then stays put like the timer
  // panel instead of stretching to fill a larger window.
  const float max_group =
      kDefaultWindowWidth * ui_scale_ - 2 * ImGui::GetStyle().WindowPadding.x;
  ui::begin_group("History", std::min(ui::avail_width(), max_group));

  draw_history_toolbar();

  const float avail = ui::avail_width();
  const float content_h = content_height(s, line_h);
  const float footer_gap = 14 * s;  // between the cells and the totals/legend line
  const ImVec2 wmin = ImGui::GetCursorScreenPos();
  const ImVec2 wmax = {wmin.x + avail, wmin.y + pad + content_h + footer_gap + line_h + pad};
  ImDrawList* dl = ImGui::GetWindowDrawList();
  dl->AddRectFilled(wmin, wmax, ui::palette().well);
  ui::draw_bevel(dl, wmin, wmax, ui::Bevel::Sunken);

  std::optional<Date> hovered;
  const Date today_day = today();
  const Well well{dl,
                  {wmin.x + pad, wmin.y + pad},
                  {wmax.x - pad, wmin.y + pad + content_h},
                  s,
                  line_h,
                  day_totals_,
                  goals_,
                  today_day,
                  config_.week_starts_on,
                  ImGui::IsWindowHovered(),
                  hovered,
                  selected_day_};
  DateSpan span{};
  switch (history_view_) {
    case HistoryView::Year: {
      draw_year(well);
      const HeatmapLayout layout = heatmap_layout(today_day, config_.week_starts_on);
      span = {layout.first_day, layout.day_at(layout.weeks - 1, 6)};
      break;
    }
    case HistoryView::Month:
      span = month_back(today_day, history_back_);
      draw_month(well, span);
      break;
    case HistoryView::Week:
      span = week_back(today_day, config_.week_starts_on, history_back_);
      draw_week(well, span);
      break;
  }

  // Footer: the period's totals (or a hint on first run) and the legend.
  const float fy = wmin.y + pad + content_h + footer_gap;
  if (day_totals_.empty()) {
    dl->AddText({wmin.x + pad, fy}, kLabel, "No sessions yet. Finished sessions show up here.");
  } else {
    draw_totals(dl, {wmin.x + pad, fy}, summarize(day_totals_, span, today_day), s);
  }
  draw_legend(dl, wmax.x - pad, fy, line_h, s);

  ImGui::Dummy({avail, wmax.y - wmin.y});
  ui::end_group();

  // Click a day to show it in the day panel; click it again, or empty space
  // in the well, to deselect.
  if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
      ImGui::IsMouseHoveringRect(wmin, wmax)) {
    if (hovered && hovered != selected_day_) {
      selected_day_ = hovered;
    } else {
      selected_day_.reset();
    }
  }
  if (hovered) draw_day_tooltip(*hovered);
}

void App::draw_day_tooltip(Date day) {
  if (!ui::begin_tooltip()) return;
  // Just the count and the date: clicking the day shows the details.
  const auto it = day_totals_.find(day);
  const int n = it == day_totals_.end() ? 0 : it->second.sessions;
  const std::string when = format_short_date(day);
  if (n == 0) {
    ImGui::Text("No sessions on %s", when.c_str());
  } else {
    ImGui::Text("%d session%s on %s", n, n == 1 ? "" : "s", when.c_str());
  }
  ui::end_tooltip();
}

}  // namespace mattrack
