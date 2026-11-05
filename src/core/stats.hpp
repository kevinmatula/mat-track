#pragma once

#include <chrono>
#include <cstdint>
#include <map>
#include <optional>
#include <vector>

#include "core/config.hpp"
#include "core/models.hpp"
#include "core/time_util.hpp"

namespace mattrack {

struct CategoryTotal {
  std::int64_t category_id = 0;
  std::int64_t seconds = 0;
  int sessions = 0;

  bool operator==(const CategoryTotal&) const = default;
};

// Totals for one study day, or for any span of days.
struct DayTotal {
  std::int64_t seconds = 0;
  int sessions = 0;
  std::vector<CategoryTotal> by_category;  // most time first, then by id

  double minutes() const { return static_cast<double>(seconds) / 60.0; }
  bool operator==(const DayTotal&) const = default;
};

// Only days with at least one session appear.
using DayTotals = std::map<Date, DayTotal>;

// No filter = all categories.
using CategoryFilter = std::optional<std::int64_t>;

// Groups sessions by study day (from start_utc; sessions are never split).
DayTotals day_totals(const std::vector<Session>& sessions, const std::chrono::time_zone& tz,
                     int rollover_hour, CategoryFilter filter = std::nullopt);

// Sum over the inclusive date span [first, last].
DayTotal total_between(const DayTotals& days, Date first, Date last);

// All-time totals per category, most time first.
std::vector<CategoryTotal> category_totals(const std::vector<Session>& sessions,
                                           CategoryFilter filter = std::nullopt);

// First and last day of the week / month containing `d`.
struct DateSpan {
  Date first;
  Date last;
  bool operator==(const DateSpan&) const = default;
};
Date week_start(Date d, WeekStart starts_on);
DateSpan week_of(Date d, WeekStart starts_on);
DateSpan month_of(Date d);

// The week / month `back` periods before the one containing `d`
// (back = 0 is the current one).
DateSpan week_back(Date d, WeekStart starts_on, int back);
DateSpan month_back(Date d, int back);

// Totals over a span, counting only days up to and including `today`.
struct PeriodSummary {
  std::int64_t total_seconds = 0;
  int days_studied = 0;  // days with any study
  int days_elapsed = 0;  // days of the span that are <= today
  Date best_day{};       // valid only if days_studied > 0
  std::int64_t best_seconds = 0;

  // Per elapsed day, so a period in progress isn't diluted by future days.
  std::int64_t average_seconds() const {
    return days_elapsed > 0 ? total_seconds / days_elapsed : 0;
  }
  bool operator==(const PeriodSummary&) const = default;
};
PeriodSummary summarize(const DayTotals& days, DateSpan span, Date today);

}  // namespace mattrack
