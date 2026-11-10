#include <catch2/catch_test_macros.hpp>

#include "core/db.hpp"
#include "core/gamification.hpp"
#include "core/goals.hpp"

using namespace mattrack;
using namespace std::chrono;

namespace {
Date ymd(int y, unsigned m, unsigned d) { return Date{year{y}, month{m}, day{d}}; }
}  // namespace

TEST_CASE("goal in effect on each day", "[goals]") {
  SECTION("no changes: the fallback everywhere") {
    const GoalHistory g({}, 120);
    CHECK(g.on(ymd(2020, 1, 1)) == 120);
    CHECK(g.latest() == 120);
  }
  SECTION("changes apply from their day on, and the past keeps the old goal") {
    // Given out of order on purpose.
    const GoalHistory g({{ymd(2026, 11, 9), 60}, {ymd(2026, 6, 1), 120}, {ymd(2026, 9, 1), 90}},
                        999);
    CHECK(g.on(ymd(2026, 1, 1)) == 120);    // before the first change: the first goal
    CHECK(g.on(ymd(2026, 6, 1)) == 120);    // a change applies on its own day
    CHECK(g.on(ymd(2026, 8, 31)) == 120);
    CHECK(g.on(ymd(2026, 9, 1)) == 90);
    CHECK(g.on(ymd(2026, 11, 8)) == 90);    // the day before a change: unchanged
    CHECK(g.on(ymd(2026, 11, 9)) == 60);
    CHECK(g.on(ymd(2030, 1, 1)) == 60);
    CHECK(g.latest() == 60);
  }
}

TEST_CASE("weekly grade uses each day's own goal", "[goals][grade]") {
  const Date mon = ymd(2026, 11, 2);
  DayTotals days;
  days[mon] = DayTotal{60 * 60, 1, {}};               // 60 min
  days[add_days(mon, 1)] = DayTotal{60 * 60, 1, {}};  // 60 min
  const Date tue = add_days(mon, 1);

  // Goal 120 all week: (0.5 + 0.5) / 2 = 0.5 -> C.
  CHECK(weekly_grade(days, tue, WeekStart::Monday, 120) == Grade::C);
  // Goal lowered to 60 on Tuesday: (0.5 + 1.0) / 2 = 0.75 -> B.
  const GoalHistory g({{mon, 120}, {tue, 60}}, 120);
  CHECK(weekly_grade(days, tue, WeekStart::Monday, g) == Grade::B);
}

TEST_CASE("sync_goal never regrades the past", "[goals][db]") {
  Db db(":memory:");
  const Date d1 = ymd(2026, 11, 1);
  const Date d2 = ymd(2026, 11, 9);

  // First run: the configured goal covers all past days.
  sync_goal(db, d1, 120);
  const GoalHistory first(db.goal_changes(), 0);
  CHECK(first.on(ymd(2020, 5, 5)) == 120);

  // Unchanged goal: nothing new recorded.
  sync_goal(db, d1, 120);
  CHECK(db.goal_changes().size() == 1);

  // Changed (twice on the same day): only today and later move.
  sync_goal(db, d2, 60);
  sync_goal(db, d2, 30);
  const GoalHistory g(db.goal_changes(), 0);
  CHECK(g.on(ymd(2026, 6, 1)) == 120);
  CHECK(g.on(add_days(d2, -1)) == 120);
  CHECK(g.on(d2) == 30);
  CHECK(g.latest() == 30);
  CHECK(db.goal_changes().size() == 2);
}
