#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "core/models.hpp"

namespace mattrack {

// A calendar date. When produced by day_key() it is a "study day": the
// local day shifted back by the rollover hour, so 01:30 still counts as the
// previous date.
using Date = std::chrono::year_month_day;

constexpr int kMinRolloverHour = 0;
constexpr int kMaxRolloverHour = 12;

// Half-open UTC interval [start, end).
struct UtcRange {
  UnixSeconds start = 0;
  UnixSeconds end = 0;

  bool contains(UnixSeconds t) const { return t >= start && t < end; }
  bool operator==(const UtcRange&) const = default;
};

// Study day that a moment belongs to: convert to local time in `tz`,
// subtract `rollover_hour` hours, take the calendar date.
// Throws std::invalid_argument if rollover_hour is outside [0, 12].
Date day_key(UnixSeconds utc, const std::chrono::time_zone& tz, int rollover_hour);

// UTC range covering a study day: from rollover_hour:00 local on `day` to
// rollover_hour:00 local on the following day. Usually 24h, but 23h/25h on
// DST transition days. If the boundary local time does not exist (DST gap)
// the range starts at the transition; if it is ambiguous (DST overlap) the
// earlier occurrence is used. Consistent with day_key(): for every t,
// day_utc_range(day_key(t)).contains(t).
UtcRange day_utc_range(Date day, const std::chrono::time_zone& tz, int rollover_hour);

Date add_days(Date d, int days);

// "2h 15m", "45m", "0m". Seconds are truncated.
std::string format_duration(std::int64_t seconds);

// "YYYY-MM-DD"
std::string format_date(Date d);
std::optional<Date> parse_date(std::string_view s);

}  // namespace mattrack
