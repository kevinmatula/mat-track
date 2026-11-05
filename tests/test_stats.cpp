#include <catch2/catch_test_macros.hpp>

#include "core/stats.hpp"

using namespace mattrack;
using namespace std::chrono;

namespace {

const time_zone& ny() { return *locate_zone("America/New_York"); }
constexpr int kRollover = 4;

Date ymd(int y, unsigned m, unsigned d) { return Date{year{y}, month{m}, day{d}}; }

// A session starting at local time h:mm on `date` in New York.
Session at(Date date, int h, int m, std::int64_t minutes, std::int64_t category = 1) {
  const auto local = local_days{date} + hours{h} + std::chrono::minutes{m};
  Session s;
  s.start_utc = ny().to_sys(local, choose::earliest).time_since_epoch().count();
  s.duration_sec = minutes * 60;
  s.end_utc = s.start_utc + s.duration_sec;
  s.category_id = category;
  return s;
}

}  // namespace

TEST_CASE("day_totals groups by study day", "[stats]") {
  const Date d = ymd(2024, 6, 15);
  const std::vector<Session> sessions = {
      at(d, 9, 0, 50, 1),
      at(d, 14, 0, 30, 2),
      at(d, 23, 50, 40, 1),           // late evening: same day
      at(ymd(2024, 6, 16), 1, 30, 20, 2),  // 1:30am: still the 15th (rollover 4)
      at(ymd(2024, 6, 16), 4, 0, 10, 1),   // 4:00am: the 16th
  };
  const DayTotals days = day_totals(sessions, ny(), kRollover);
  REQUIRE(days.size() == 2);

  const DayTotal& t = days.at(d);
  CHECK(t.seconds == (50 + 30 + 40 + 20) * 60);
  CHECK(t.sessions == 4);
  CHECK(t.minutes() == 140);
  CHECK(t.by_category == std::vector<CategoryTotal>{{1, 90 * 60, 2}, {2, 50 * 60, 2}});

  CHECK(days.at(ymd(2024, 6, 16)).seconds == 10 * 60);
}

TEST_CASE("a long session is not split across days", "[stats]") {
  const DayTotals days = day_totals({at(ymd(2024, 6, 16), 3, 0, 180)}, ny(), kRollover);
  REQUIRE(days.size() == 1);
  CHECK(days.at(ymd(2024, 6, 15)).seconds == 180 * 60);
}

TEST_CASE("rollover setting re-derives history", "[stats]") {
  const std::vector<Session> sessions = {at(ymd(2024, 6, 16), 1, 30, 20)};
  CHECK(day_totals(sessions, ny(), 4).count(ymd(2024, 6, 15)) == 1);
  CHECK(day_totals(sessions, ny(), 0).count(ymd(2024, 6, 16)) == 1);
}

TEST_CASE("category filter", "[stats]") {
  const Date d = ymd(2024, 6, 15);
  const std::vector<Session> sessions = {at(d, 9, 0, 50, 1), at(d, 10, 0, 30, 2),
                                         at(ymd(2024, 6, 17), 9, 0, 25, 2)};
  const DayTotals only2 = day_totals(sessions, ny(), kRollover, 2);
  REQUIRE(only2.size() == 2);
  CHECK(only2.at(d).seconds == 30 * 60);
  CHECK(only2.at(d).sessions == 1);
  CHECK(only2.at(d).by_category == std::vector<CategoryTotal>{{2, 30 * 60, 1}});

  const DayTotals only1 = day_totals(sessions, ny(), kRollover, 1);
  CHECK(only1.size() == 1);

  CHECK(day_totals(sessions, ny(), kRollover, 99).empty());
}

TEST_CASE("per-category totals: sorted by time, ties by id", "[stats]") {
  const Date d = ymd(2024, 6, 15);
  const std::vector<Session> sessions = {at(d, 9, 0, 10, 3), at(d, 10, 0, 30, 1),
                                         at(d, 11, 0, 20, 2), at(d, 12, 0, 20, 3)};
  CHECK(category_totals(sessions) ==
        std::vector<CategoryTotal>{{1, 30 * 60, 1}, {3, 30 * 60, 2}, {2, 20 * 60, 1}});
  CHECK(category_totals(sessions, 2) == std::vector<CategoryTotal>{{2, 20 * 60, 1}});
  CHECK(category_totals({}).empty());
}

TEST_CASE("week boundaries", "[stats]") {
  const Date wed = ymd(2024, 6, 12);  // a Wednesday
  CHECK(week_start(wed, WeekStart::Monday) == ymd(2024, 6, 10));
  CHECK(week_start(wed, WeekStart::Sunday) == ymd(2024, 6, 9));

  const Date sun = ymd(2024, 6, 16);
  CHECK(week_start(sun, WeekStart::Monday) == ymd(2024, 6, 10));  // end of Mon-week
  CHECK(week_start(sun, WeekStart::Sunday) == sun);               // start of Sun-week
  const Date mon = ymd(2024, 6, 17);
  CHECK(week_start(mon, WeekStart::Monday) == mon);
  CHECK(week_start(mon, WeekStart::Sunday) == sun);

  // Across a month and year boundary.
  CHECK(week_of(ymd(2025, 1, 1), WeekStart::Monday) ==
        DateSpan{ymd(2024, 12, 30), ymd(2025, 1, 5)});
}

TEST_CASE("month boundaries", "[stats]") {
  CHECK(month_of(ymd(2024, 2, 10)) == DateSpan{ymd(2024, 2, 1), ymd(2024, 2, 29)});
  CHECK(month_of(ymd(2023, 2, 28)) == DateSpan{ymd(2023, 2, 1), ymd(2023, 2, 28)});
  CHECK(month_of(ymd(2024, 12, 31)) == DateSpan{ymd(2024, 12, 1), ymd(2024, 12, 31)});
}

TEST_CASE("total_between sums an inclusive span", "[stats]") {
  const std::vector<Session> sessions = {
      at(ymd(2024, 6, 9), 9, 0, 10, 1),   // Sunday before
      at(ymd(2024, 6, 10), 9, 0, 20, 1),  // Monday
      at(ymd(2024, 6, 12), 9, 0, 30, 2),
      at(ymd(2024, 6, 17), 1, 0, 40, 2),  // 1am Monday = Sunday the 16th's study day
      at(ymd(2024, 6, 17), 9, 0, 50, 1),  // next week
  };
  const DayTotals days = day_totals(sessions, ny(), kRollover);
  const DateSpan week = week_of(ymd(2024, 6, 12), WeekStart::Monday);
  const DayTotal t = total_between(days, week.first, week.last);
  CHECK(t.seconds == (20 + 30 + 40) * 60);
  CHECK(t.sessions == 3);
  CHECK(t.by_category == std::vector<CategoryTotal>{{2, 70 * 60, 2}, {1, 20 * 60, 1}});

  const DateSpan june = month_of(ymd(2024, 6, 1));
  CHECK(total_between(days, june.first, june.last).sessions == 5);
  CHECK(total_between(days, ymd(2024, 7, 1), ymd(2024, 7, 31)) == DayTotal{});
}

TEST_CASE("stepping back by weeks and months", "[stats]") {
  const Date d = ymd(2026, 11, 4);  // Wednesday
  CHECK(week_back(d, WeekStart::Monday, 0) == DateSpan{ymd(2026, 11, 2), ymd(2026, 11, 8)});
  CHECK(week_back(d, WeekStart::Monday, 1) == DateSpan{ymd(2026, 10, 26), ymd(2026, 11, 1)});
  CHECK(week_back(d, WeekStart::Sunday, 1) == DateSpan{ymd(2026, 10, 25), ymd(2026, 10, 31)});

  CHECK(month_back(d, 0) == DateSpan{ymd(2026, 11, 1), ymd(2026, 11, 30)});
  CHECK(month_back(d, 1) == DateSpan{ymd(2026, 10, 1), ymd(2026, 10, 31)});
  CHECK(month_back(d, 9) == DateSpan{ymd(2026, 2, 1), ymd(2026, 2, 28)});
  CHECK(month_back(d, 11) == DateSpan{ymd(2025, 12, 1), ymd(2025, 12, 31)});  // across a year
  CHECK(month_back(ymd(2024, 3, 31), 1) == DateSpan{ymd(2024, 2, 1), ymd(2024, 2, 29)});
}

TEST_CASE("summarize a period", "[stats]") {
  const std::vector<Session> sessions = {
      at(ymd(2026, 11, 2), 9, 0, 60),   // Mon
      at(ymd(2026, 11, 2), 14, 0, 30),  // Mon: 90 total
      at(ymd(2026, 11, 3), 9, 0, 120),  // Tue: best
      at(ymd(2026, 11, 7), 9, 0, 45),   // Sat: after "today"
      at(ymd(2026, 10, 31), 9, 0, 300), // previous week
  };
  const DayTotals days = day_totals(sessions, ny(), kRollover);
  const DateSpan week = week_of(ymd(2026, 11, 4), WeekStart::Monday);

  SECTION("week in progress: future days are ignored") {
    const PeriodSummary p = summarize(days, week, ymd(2026, 11, 4));
    CHECK(p.total_seconds == 210 * 60);
    CHECK(p.days_studied == 2);
    CHECK(p.days_elapsed == 3);  // Mon..Wed
    CHECK(p.average_seconds() == 70 * 60);
    CHECK(p.best_day == ymd(2026, 11, 3));
    CHECK(p.best_seconds == 120 * 60);
  }
  SECTION("finished week counts all seven days") {
    const PeriodSummary p = summarize(days, week, ymd(2026, 12, 1));
    CHECK(p.total_seconds == 255 * 60);
    CHECK(p.days_elapsed == 7);
    CHECK(p.days_studied == 3);
  }
  SECTION("a future period is empty") {
    const PeriodSummary p = summarize(days, week, ymd(2026, 10, 1));
    CHECK(p == PeriodSummary{});
    CHECK(p.average_seconds() == 0);
  }
  SECTION("no data") {
    const PeriodSummary p = summarize({}, week, ymd(2026, 11, 4));
    CHECK(p.days_elapsed == 3);
    CHECK(p.days_studied == 0);
    CHECK(p.total_seconds == 0);
  }
}
