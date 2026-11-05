#include "core/heatmap.hpp"

#include <algorithm>

#include "core/stats.hpp"

namespace mattrack {

namespace chr = std::chrono;

int intensity_level(std::int64_t seconds, int daily_goal_minutes) {
  if (seconds <= 0) return 0;
  // Exact integer quarters of the goal, as for grades.
  const std::int64_t goal = std::int64_t{std::max(1, daily_goal_minutes)} * 60;
  const std::int64_t q = seconds * 4;
  if (q < goal) return 1;
  if (q < 2 * goal) return 2;
  if (q < 4 * goal) return 3;
  return 4;
}

int HeatmapLayout::row_of(chr::weekday wd) const {
  return static_cast<int>(starts_on == WeekStart::Monday ? wd.iso_encoding() - 1
                                                         : wd.c_encoding());
}

HeatmapLayout heatmap_layout(Date today, WeekStart starts_on, int weeks) {
  weeks = std::max(1, weeks);
  return {add_days(week_start(today, starts_on), -7 * (weeks - 1)), weeks, starts_on};
}

std::vector<MonthStart> month_starts(const HeatmapLayout& layout) {
  std::vector<MonthStart> out;
  for (int col = 0; col < layout.weeks; ++col) {
    for (int row = 0; row < 7; ++row) {
      const Date d = layout.day_at(col, row);
      if (d.day() == chr::day{1}) {
        out.push_back({col, d.month()});
        break;
      }
    }
  }
  return out;
}

}  // namespace mattrack
