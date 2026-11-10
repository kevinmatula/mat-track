#pragma once

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

#include "core/goals.hpp"
#include "core/stats.hpp"

// Everything here is derived from the session log on demand; nothing is
// stored, so formulas can change freely.
namespace mattrack {

// ---- grades -----------------------------------------------------------------

enum class Grade { None, F, D, C, B, A, S };

// r = minutes / goal. 0 -> None ("-").
Grade grade_for_ratio(double ratio);
Grade daily_grade(std::int64_t seconds, int daily_goal_minutes);
std::string_view grade_label(Grade g);  // "-", "F", ... "S"

constexpr double kWeeklyRatioCap = 1.5;

// Average of daily ratios (each capped at 1.5) over the days of the week
// containing `today` up to and including `today`, so a week in progress is
// not penalised for days that have not happened yet.
// Each day's ratio uses the goal in effect that day.
Grade weekly_grade(const DayTotals& days, Date today, WeekStart starts_on,
                   const GoalHistory& goals);
inline Grade weekly_grade(const DayTotals& days, Date today, WeekStart starts_on,
                          int daily_goal_minutes) {
  return weekly_grade(days, today, starts_on, GoalHistory(daily_goal_minutes));
}

// ---- XP and levels ----------------------------------------------------------

// 1 XP per full minute studied.
std::int64_t xp_for_seconds(std::int64_t total_seconds);
// Cumulative XP needed to reach `level`: 50 * level^2.
std::int64_t level_threshold(int level);
// Largest level whose threshold is <= xp (0 below 50 XP).
int level_for_xp(std::int64_t xp);
std::string_view rank_title(int level);

struct LevelProgress {
  int level = 0;
  std::int64_t xp = 0;
  std::int64_t level_xp = 0;  // threshold of the current level
  std::int64_t next_xp = 0;   // threshold of the next level
  // 0..1 progress from the current level to the next.
  double fraction() const {
    return static_cast<double>(xp - level_xp) / static_cast<double>(next_xp - level_xp);
  }
  bool operator==(const LevelProgress&) const = default;
};
LevelProgress level_progress(std::int64_t xp);

// ---- streaks ----------------------------------------------------------------

// Consecutive streak days (>= streak_min_minutes) ending today, or ending
// yesterday if today has not met the threshold yet.
int current_streak(const DayTotals& days, Date today, int streak_min_minutes);
int longest_streak(const DayTotals& days, int streak_min_minutes);

// ---- records ----------------------------------------------------------------

struct DayRecord {
  Date day;
  std::int64_t seconds = 0;
  bool operator==(const DayRecord&) const = default;
};

struct SessionRecord {
  std::int64_t session_id = 0;
  Date day;  // study day of the session
  std::int64_t seconds = 0;
  bool operator==(const SessionRecord&) const = default;
};

// On ties the earliest holder keeps the record.
struct Records {
  std::optional<DayRecord> best_day;
  std::optional<SessionRecord> longest_session;
  std::optional<DayRecord> best_week;  // day = first day of the week
  int longest_streak = 0;
  std::int64_t total_seconds = 0;
  int total_sessions = 0;
  bool operator==(const Records&) const = default;
};

struct RecordSettings {
  const std::chrono::time_zone* tz = nullptr;
  int rollover_hour = 4;
  WeekStart week_starts_on = WeekStart::Monday;
  int streak_min_minutes = 30;
};

Records compute_records(const std::vector<Session>& sessions, const RecordSettings& settings);

enum class RecordKind { BestDay, LongestSession, BestWeek, LongestStreak };

// Records in `after` that strictly beat `before` (e.g. after logging a
// session), for the "NEW RECORD" banner. The first record ever set does
// not count as new.
std::vector<RecordKind> new_records(const Records& before, const Records& after);

}  // namespace mattrack
