#include <catch2/catch_test_macros.hpp>

#include "core/heatmap.hpp"

using namespace mattrack;
using namespace std::chrono;

namespace {
Date ymd(int y, unsigned m, unsigned d) { return Date{year{y}, month{m}, day{d}}; }
}  // namespace

TEST_CASE("intensity levels at every boundary (goal 120)", "[heatmap]") {
  auto lv = [](std::int64_t minutes_x60) { return intensity_level(minutes_x60, 120); };
  CHECK(lv(0) == 0);
  CHECK(lv(1) == 1);
  CHECK(lv(30 * 60 - 1) == 1);
  CHECK(lv(30 * 60) == 2);  // 25%
  CHECK(lv(60 * 60 - 1) == 2);
  CHECK(lv(60 * 60) == 3);  // 50%
  CHECK(lv(120 * 60 - 1) == 3);
  CHECK(lv(120 * 60) == 4);  // 100%
  CHECK(lv(600 * 60) == 4);
  CHECK(intensity_level(-5, 120) == 0);
}

TEST_CASE("layout ends with the week containing today", "[heatmap]") {
  const Date today = ymd(2026, 11, 4);  // a Wednesday

  SECTION("Monday weeks") {
    const HeatmapLayout l = heatmap_layout(today, WeekStart::Monday);
    CHECK(l.weeks == 53);
    CHECK(l.day_at(52, 0) == ymd(2026, 11, 2));  // this Monday
    CHECK(l.day_at(52, 2) == today);
    CHECK(l.day_at(52, 6) == ymd(2026, 11, 8));  // Sunday, in the future
    CHECK(l.first_day == ymd(2025, 11, 3));      // 52 weeks earlier, a Monday
    CHECK(l.row_of(weekday{sys_days{today}}) == 2);
    CHECK(l.row_of(Sunday) == 6);
  }
  SECTION("Sunday weeks") {
    const HeatmapLayout l = heatmap_layout(today, WeekStart::Sunday);
    CHECK(l.day_at(52, 0) == ymd(2026, 11, 1));  // this Sunday
    CHECK(l.day_at(52, 3) == today);
    CHECK(l.row_of(Sunday) == 0);
    CHECK(l.row_of(Monday) == 1);
  }
  SECTION("today on the first day of the week") {
    const HeatmapLayout l = heatmap_layout(ymd(2026, 11, 2), WeekStart::Monday);
    CHECK(l.day_at(52, 0) == ymd(2026, 11, 2));
  }
  SECTION("today on the last day of the week") {
    const HeatmapLayout l = heatmap_layout(ymd(2026, 11, 8), WeekStart::Monday);
    CHECK(l.day_at(52, 6) == ymd(2026, 11, 8));
  }
}

TEST_CASE("every cell is a distinct consecutive day", "[heatmap]") {
  const HeatmapLayout l = heatmap_layout(ymd(2024, 3, 1), WeekStart::Monday);
  Date expected = l.first_day;
  for (int col = 0; col < l.weeks; ++col) {
    for (int row = 0; row < 7; ++row) {
      REQUIRE(l.day_at(col, row) == expected);
      expected = add_days(expected, 1);
    }
  }
}

TEST_CASE("month labels go where each month's 1st falls", "[heatmap]") {
  const HeatmapLayout l = heatmap_layout(ymd(2026, 11, 4), WeekStart::Monday);
  const auto starts = month_starts(l);
  // Nov 2025 starts before the grid (first day Nov 3), so Dec is first.
  REQUIRE(starts.size() == 12);
  CHECK(starts.front().month == December);
  CHECK(starts.back().month == November);
  for (const auto& s : starts) {
    bool found = false;
    for (int row = 0; row < 7; ++row) found |= l.day_at(s.column, row).day() == day{1};
    CHECK(found);
  }
  // Columns strictly increase.
  for (std::size_t i = 1; i < starts.size(); ++i) CHECK(starts[i].column > starts[i - 1].column);
}

TEST_CASE("a grid starting on the 1st labels its first column", "[heatmap]") {
  // 2024-01-01 is a Monday.
  const HeatmapLayout l = heatmap_layout(ymd(2024, 1, 1), WeekStart::Monday, 1);
  CHECK(month_starts(l) == std::vector<MonthStart>{{0, January}});
}
