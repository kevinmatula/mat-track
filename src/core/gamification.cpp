#include "core/gamification.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace mattrack {

// ---- grades -----------------------------------------------------------------

Grade grade_for_ratio(double r) {
  if (r <= 0) return Grade::None;
  if (r < 0.25) return Grade::F;
  if (r < 0.50) return Grade::D;
  if (r < 0.75) return Grade::C;
  if (r < 1.00) return Grade::B;
  if (r < 1.50) return Grade::A;
  return Grade::S;
}

Grade daily_grade(std::int64_t seconds, int daily_goal_minutes) {
  // Same table as grade_for_ratio, in exact integer quarters of the goal so
  // boundaries like "exactly 30 of 120 minutes" can't be lost to rounding.
  const std::int64_t goal = std::int64_t{std::max(1, daily_goal_minutes)} * 60;
  const std::int64_t q = seconds * 4;
  if (seconds <= 0) return Grade::None;
  if (q < goal) return Grade::F;
  if (q < 2 * goal) return Grade::D;
  if (q < 3 * goal) return Grade::C;
  if (q < 4 * goal) return Grade::B;
  if (q < 6 * goal) return Grade::A;
  return Grade::S;
}

std::string_view grade_label(Grade g) {
  switch (g) {
    case Grade::None: return "-";
    case Grade::F: return "F";
    case Grade::D: return "D";
    case Grade::C: return "C";
    case Grade::B: return "B";
    case Grade::A: return "A";
    case Grade::S: return "S";
  }
  return "-";
}

Grade weekly_grade(const DayTotals& days, Date today, WeekStart starts_on,
                   int daily_goal_minutes) {
  const double goal_seconds = std::max(1, daily_goal_minutes) * 60.0;
  double sum = 0;
  int count = 0;
  for (Date d = week_start(today, starts_on); d <= today; d = add_days(d, 1)) {
    const auto it = days.find(d);
    const double secs = it == days.end() ? 0.0 : static_cast<double>(it->second.seconds);
    sum += std::min(secs / goal_seconds, kWeeklyRatioCap);
    ++count;
  }
  return grade_for_ratio(sum / count);
}

// ---- XP and levels ----------------------------------------------------------

namespace {

struct Rank {
  int min_level;
  std::string_view title;
};

// Editable: each title applies from min_level up to the next entry.
constexpr std::array kRanks = {
    Rank{0, "Novice"},  Rank{5, "Apprentice"}, Rank{10, "Scholar"}, Rank{15, "Adept"},
    Rank{20, "Expert"}, Rank{30, "Master"},     Rank{40, "Sage"},
};

}  // namespace

std::int64_t xp_for_seconds(std::int64_t total_seconds) {
  return std::max<std::int64_t>(0, total_seconds / 60);
}

std::int64_t level_threshold(int level) {
  return 50 * std::int64_t{level} * level;
}

int level_for_xp(std::int64_t xp) {
  if (xp < 50) return 0;
  int level = static_cast<int>(std::sqrt(static_cast<double>(xp) / 50.0));
  // Correct any floating-point error at exact boundaries.
  while (level_threshold(level + 1) <= xp) ++level;
  while (level_threshold(level) > xp) --level;
  return level;
}

std::string_view rank_title(int level) {
  std::string_view title = kRanks.front().title;
  for (const auto& r : kRanks) {
    if (level >= r.min_level) title = r.title;
  }
  return title;
}

LevelProgress level_progress(std::int64_t xp) {
  xp = std::max<std::int64_t>(0, xp);
  const int level = level_for_xp(xp);
  return {level, xp, level_threshold(level), level_threshold(level + 1)};
}

// ---- streaks ----------------------------------------------------------------

namespace {

bool is_streak_day(const DayTotals& days, Date d, int streak_min_minutes) {
  const auto it = days.find(d);
  return it != days.end() && it->second.seconds >= std::int64_t{streak_min_minutes} * 60;
}

}  // namespace

int current_streak(const DayTotals& days, Date today, int streak_min_minutes) {
  Date d = is_streak_day(days, today, streak_min_minutes) ? today : add_days(today, -1);
  int streak = 0;
  while (is_streak_day(days, d, streak_min_minutes)) {
    ++streak;
    d = add_days(d, -1);
  }
  return streak;
}

int longest_streak(const DayTotals& days, int streak_min_minutes) {
  int best = 0;
  int run = 0;
  std::optional<Date> prev;
  for (const auto& [day, total] : days) {
    if (total.seconds < std::int64_t{streak_min_minutes} * 60) {
      run = 0;
      prev.reset();
      continue;
    }
    run = (prev && add_days(*prev, 1) == day) ? run + 1 : 1;
    prev = day;
    best = std::max(best, run);
  }
  return best;
}

// ---- records ----------------------------------------------------------------

Records compute_records(const std::vector<Session>& sessions, const RecordSettings& settings) {
  Records r;
  const DayTotals days = day_totals(sessions, *settings.tz, settings.rollover_hour);

  std::map<Date, std::int64_t> weeks;
  for (const auto& [day, total] : days) {
    // Days are visited in date order, so ">" keeps the earliest on ties.
    if (!r.best_day || total.seconds > r.best_day->seconds) r.best_day = DayRecord{day, total.seconds};
    weeks[week_start(day, settings.week_starts_on)] += total.seconds;
  }
  for (const auto& [first_day, seconds] : weeks) {
    if (!r.best_week || seconds > r.best_week->seconds) r.best_week = DayRecord{first_day, seconds};
  }

  const Session* longest = nullptr;
  for (const auto& s : sessions) {
    r.total_seconds += s.duration_sec;
    r.total_sessions += 1;
    if (!longest || s.duration_sec > longest->duration_sec ||
        (s.duration_sec == longest->duration_sec && s.start_utc < longest->start_utc)) {
      longest = &s;
    }
  }
  if (longest) {
    r.longest_session = SessionRecord{
        longest->id, day_key(longest->start_utc, *settings.tz, settings.rollover_hour),
        longest->duration_sec};
  }

  r.longest_streak = longest_streak(days, settings.streak_min_minutes);
  return r;
}

std::vector<RecordKind> new_records(const Records& before, const Records& after) {
  std::vector<RecordKind> out;
  auto beaten = [](const auto& old_rec, const auto& new_rec) {
    return old_rec && new_rec && new_rec->seconds > old_rec->seconds;
  };
  if (beaten(before.best_day, after.best_day)) out.push_back(RecordKind::BestDay);
  if (beaten(before.longest_session, after.longest_session)) {
    out.push_back(RecordKind::LongestSession);
  }
  if (beaten(before.best_week, after.best_week)) out.push_back(RecordKind::BestWeek);
  if (before.longest_streak > 0 && after.longest_streak > before.longest_streak) {
    out.push_back(RecordKind::LongestStreak);
  }
  return out;
}

}  // namespace mattrack
