#pragma once

#include <cstdint>
#include <vector>

#include "core/config.hpp"
#include "core/time_util.hpp"

// Layout and shading for the year heatmap: columns are weeks (oldest on the
// left, the current study week on the right), rows are weekdays.
namespace mattrack {

constexpr int kHeatmapWeeks = 53;
constexpr int kHeatmapLevels = 5;

// 0 = no study; 1..4 by share of the daily goal: >0, >=25%, >=50%, >=100%.
int intensity_level(std::int64_t seconds, int daily_goal_minutes);

struct HeatmapLayout {
  Date first_day;  // top-left cell: the first day of the oldest week
  int weeks = kHeatmapWeeks;
  WeekStart starts_on = WeekStart::Monday;

  Date day_at(int column, int row) const { return add_days(first_day, column * 7 + row); }
  // Row of a weekday: 0 is the week's first day.
  int row_of(std::chrono::weekday wd) const;
};

// `weeks` columns ending with the week that contains `today`.
HeatmapLayout heatmap_layout(Date today, WeekStart starts_on, int weeks = kHeatmapWeeks);

// Columns in which a new month starts (the column holding its 1st), for the
// month labels. The first column is included only if its first day is a 1st.
struct MonthStart {
  int column;
  std::chrono::month month;
  bool operator==(const MonthStart&) const = default;
};
std::vector<MonthStart> month_starts(const HeatmapLayout& layout);

}  // namespace mattrack
