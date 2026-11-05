#include <catch2/catch_test_macros.hpp>

#include "core/gamification.hpp"

using namespace mattrack;
using namespace std::chrono;

namespace {

const time_zone& ny() { return *locate_zone("America/New_York"); }

Date ymd(int y, unsigned m, unsigned d) { return Date{year{y}, month{m}, day{d}}; }

// DayTotals with the given minutes on consecutive days starting at `first`.
DayTotals days_from(Date first, std::vector<int> minutes) {
  DayTotals days;
  for (std::size_t i = 0; i < minutes.size(); ++i) {
    if (minutes[i] > 0) {
      days[add_days(first, static_cast<int>(i))] = DayTotal{minutes[i] * 60, 1, {}};
    }
  }
  return days;
}

Session at(Date date, int h, std::int64_t minutes, std::int64_t id) {
  Session s;
  s.id = id;
  s.start_utc = ny().to_sys(local_days{date} + hours{h}, choose::earliest).time_since_epoch().count();
  s.duration_sec = minutes * 60;
  s.end_utc = s.start_utc + s.duration_sec;
  s.category_id = 1;
  return s;
}

RecordSettings record_settings() {
  return {.tz = &ny(), .rollover_hour = 4, .week_starts_on = WeekStart::Monday,
          .streak_min_minutes = 30};
}

}  // namespace

TEST_CASE("daily grade at every boundary (goal 120)", "[gamification][grade]") {
  auto g = [](std::int64_t seconds) { return daily_grade(seconds, 120); };
  CHECK(g(0) == Grade::None);
  CHECK(g(1) == Grade::F);
  CHECK(g(30 * 60 - 1) == Grade::F);
  CHECK(g(30 * 60) == Grade::D);  // exactly 0.25
  CHECK(g(60 * 60 - 1) == Grade::D);
  CHECK(g(60 * 60) == Grade::C);  // 0.50
  CHECK(g(90 * 60 - 1) == Grade::C);
  CHECK(g(90 * 60) == Grade::B);  // 0.75
  CHECK(g(120 * 60 - 1) == Grade::B);
  CHECK(g(120 * 60) == Grade::A);  // 1.00
  CHECK(g(180 * 60 - 1) == Grade::A);
  CHECK(g(180 * 60) == Grade::S);  // 1.50
  CHECK(g(1000 * 60) == Grade::S);
}

TEST_CASE("grade_for_ratio boundaries and labels", "[gamification][grade]") {
  CHECK(grade_for_ratio(0) == Grade::None);
  CHECK(grade_for_ratio(0.001) == Grade::F);
  CHECK(grade_for_ratio(0.25) == Grade::D);
  CHECK(grade_for_ratio(0.5) == Grade::C);
  CHECK(grade_for_ratio(0.75) == Grade::B);
  CHECK(grade_for_ratio(1.0) == Grade::A);
  CHECK(grade_for_ratio(1.5) == Grade::S);
  CHECK(grade_label(Grade::None) == "-");
  CHECK(grade_label(Grade::S) == "S");
  CHECK(grade_label(Grade::F) == "F");
}

TEST_CASE("daily grade with an odd goal stays exact", "[gamification][grade]") {
  // goal 50: 0.25 = 12.5 min = 750 s
  CHECK(daily_grade(749, 50) == Grade::F);
  CHECK(daily_grade(750, 50) == Grade::D);
}

TEST_CASE("weekly grade averages elapsed days, capped at 1.5", "[gamification][grade]") {
  const Date mon = ymd(2024, 6, 10);
  SECTION("partial week only counts days so far") {
    // Mon 120, Tue 120, today Wed 0 -> avg 2/3 -> C
    const DayTotals days = days_from(mon, {120, 120});
    CHECK(weekly_grade(days, add_days(mon, 2), WeekStart::Monday, 120) == Grade::C);
    CHECK(weekly_grade(days, add_days(mon, 1), WeekStart::Monday, 120) == Grade::A);
  }
  SECTION("one huge day is capped") {
    // Mon 600 (ratio 5, capped 1.5), Tue 0 -> avg 0.75 -> B
    const DayTotals days = days_from(mon, {600});
    CHECK(weekly_grade(days, add_days(mon, 1), WeekStart::Monday, 120) == Grade::B);
  }
  SECTION("previous week's days are excluded") {
    const DayTotals days = days_from(add_days(mon, -1), {600});  // Sunday before
    CHECK(weekly_grade(days, mon, WeekStart::Monday, 120) == Grade::None);
    // Sun 600 (capped 1.5) + Mon 0 -> avg 0.75
    CHECK(weekly_grade(days, mon, WeekStart::Sunday, 120) == Grade::B);
  }
}

TEST_CASE("XP is whole minutes", "[gamification][xp]") {
  CHECK(xp_for_seconds(0) == 0);
  CHECK(xp_for_seconds(59) == 0);
  CHECK(xp_for_seconds(60) == 1);
  CHECK(xp_for_seconds(3 * 3600 + 30) == 180);
}

TEST_CASE("level thresholds at exact boundaries", "[gamification][xp]") {
  CHECK(level_threshold(1) == 50);
  CHECK(level_threshold(5) == 1250);
  CHECK(level_threshold(10) == 5000);
  CHECK(level_threshold(20) == 20000);

  CHECK(level_for_xp(0) == 0);
  CHECK(level_for_xp(49) == 0);
  CHECK(level_for_xp(50) == 1);
  CHECK(level_for_xp(199) == 1);
  CHECK(level_for_xp(200) == 2);
  CHECK(level_for_xp(1249) == 4);
  CHECK(level_for_xp(1250) == 5);
  CHECK(level_for_xp(5000) == 10);
  CHECK(level_for_xp(19999) == 19);
  CHECK(level_for_xp(20000) == 20);
  for (int l = 1; l <= 200; ++l) {
    INFO("level " << l);
    REQUIRE(level_for_xp(level_threshold(l)) == l);
    REQUIRE(level_for_xp(level_threshold(l) - 1) == l - 1);
  }
}

TEST_CASE("level progress", "[gamification][xp]") {
  const LevelProgress p = level_progress(1500);  // L5 = 1250, L6 = 1800
  CHECK(p == LevelProgress{5, 1500, 1250, 1800});
  CHECK(p.fraction() == (1500.0 - 1250) / (1800 - 1250));
  CHECK(level_progress(0) == LevelProgress{0, 0, 0, 50});
  CHECK(level_progress(0).fraction() == 0);
}

TEST_CASE("rank titles", "[gamification][xp]") {
  CHECK(rank_title(0) == "Novice");
  CHECK(rank_title(4) == "Novice");
  CHECK(rank_title(5) == "Apprentice");
  CHECK(rank_title(10) == "Scholar");
  CHECK(rank_title(15) == "Adept");
  CHECK(rank_title(29) == "Expert");
  CHECK(rank_title(30) == "Master");
  CHECK(rank_title(40) == "Sage");
  CHECK(rank_title(999) == "Sage");
}

TEST_CASE("current streak", "[gamification][streak]") {
  const Date d0 = ymd(2024, 6, 10);
  // d0..d4: 30, 45, 10 (below 30), 60, 30
  const DayTotals days = days_from(d0, {30, 45, 10, 60, 30});

  SECTION("ending today") { CHECK(current_streak(days, add_days(d0, 4), 30) == 2); }
  SECTION("today not yet met: still counts through yesterday") {
    CHECK(current_streak(days, add_days(d0, 5), 30) == 2);
  }
  SECTION("today partially studied but below threshold") {
    CHECK(current_streak(days, add_days(d0, 2), 30) == 2);  // d0, d1
  }
  SECTION("missed yesterday: broken") { CHECK(current_streak(days, add_days(d0, 6), 30) == 0); }
  SECTION("exact threshold counts") { CHECK(current_streak(days, d0, 30) == 1); }
  SECTION("no data") { CHECK(current_streak({}, d0, 30) == 0); }
}

TEST_CASE("longest streak", "[gamification][streak]") {
  const Date d0 = ymd(2024, 6, 10);
  CHECK(longest_streak({}, 30) == 0);
  CHECK(longest_streak(days_from(d0, {30, 45, 10, 60, 30, 40}), 30) == 3);
  // Gap day with no entry at all breaks it too.
  CHECK(longest_streak(days_from(d0, {60, 60, 0, 60}), 30) == 2);
  // Ties: same length either way.
  CHECK(longest_streak(days_from(d0, {60, 60, 0, 60, 60}), 30) == 2);
  // Across a month boundary.
  CHECK(longest_streak(days_from(ymd(2024, 6, 29), {60, 60, 60, 60}), 30) == 4);
}

TEST_CASE("records", "[gamification][records]") {
  const std::vector<Session> sessions = {
      at(ymd(2024, 6, 10), 9, 50, 1),   // Mon
      at(ymd(2024, 6, 10), 14, 70, 2),  // Mon: day total 120
      at(ymd(2024, 6, 11), 9, 90, 3),   // Tue
      at(ymd(2024, 6, 17), 9, 90, 4),   // next Mon: ties longest session (later)
      at(ymd(2024, 6, 18), 9, 30, 5),
      at(ymd(2024, 6, 19), 9, 120, 6),  // ties best day (later)
  };
  const Records r = compute_records(sessions, record_settings());

  CHECK(r.best_day == DayRecord{ymd(2024, 6, 10), 120 * 60});
  CHECK(r.longest_session == SessionRecord{6, ymd(2024, 6, 19), 120 * 60});
  CHECK(r.best_week == DayRecord{ymd(2024, 6, 17), 240 * 60});  // 90+30+120 vs 210
  CHECK(r.longest_streak == 3);
  CHECK(r.total_seconds == 450 * 60);
  CHECK(r.total_sessions == 6);

  SECTION("ties go to the earliest") {
    const std::vector<Session> tied = {at(ymd(2024, 6, 12), 9, 60, 2),
                                       at(ymd(2024, 6, 11), 9, 60, 1)};
    const Records t = compute_records(tied, record_settings());
    CHECK(t.longest_session->session_id == 1);
    CHECK(t.best_day->day == ymd(2024, 6, 11));
  }
  SECTION("longest session's day uses the rollover") {
    const Records t =
        compute_records({at(ymd(2024, 6, 12), 2, 200, 9)}, record_settings());
    CHECK(t.longest_session->day == ymd(2024, 6, 11));
  }
}

TEST_CASE("no sessions: empty records", "[gamification][records]") {
  const Records r = compute_records({}, record_settings());
  CHECK(r == Records{});
}

TEST_CASE("new_records detects strictly beaten records", "[gamification][records]") {
  std::vector<Session> sessions = {at(ymd(2024, 6, 10), 9, 60, 1),
                                   at(ymd(2024, 6, 11), 9, 60, 2)};
  const Records before = compute_records(sessions, record_settings());

  SECTION("first ever session is not a 'new record'") {
    CHECK(new_records(Records{}, compute_records({sessions[0]}, record_settings())).empty());
  }
  SECTION("a longer session beats session, day and week") {
    sessions.push_back(at(ymd(2024, 6, 12), 9, 90, 3));
    const auto got = new_records(before, compute_records(sessions, record_settings()));
    CHECK(got == std::vector<RecordKind>{RecordKind::BestDay, RecordKind::LongestSession,
                                         RecordKind::BestWeek, RecordKind::LongestStreak});
  }
  SECTION("equalling a record is not a new record") {
    sessions.push_back(at(ymd(2024, 6, 20), 9, 60, 3));  // next week, 60 min
    CHECK(new_records(before, compute_records(sessions, record_settings())).empty());
  }
}
