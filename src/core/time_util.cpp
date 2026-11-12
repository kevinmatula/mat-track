#include "core/time_util.hpp"

#include <algorithm>
#include <charconv>
#include <cstdio>
#include <stdexcept>

namespace mattrack {

namespace chr = std::chrono;

namespace {

void check_rollover(int rollover_hour) {
  if (rollover_hour < kMinRolloverHour || rollover_hour > kMaxRolloverHour) {
    throw std::invalid_argument("rollover_hour must be in [0, 12], got " +
                                std::to_string(rollover_hour));
  }
}

UnixSeconds local_to_utc(chr::local_seconds local, const chr::time_zone& tz) {
  // choose::earliest picks the first occurrence of an ambiguous time; for a
  // nonexistent time both choices yield the transition instant.
  return tz.to_sys(local, chr::choose::earliest).time_since_epoch().count();
}

}  // namespace

Date day_key(UnixSeconds utc, const chr::time_zone& tz, int rollover_hour) {
  check_rollover(rollover_hour);
  const chr::local_seconds local = tz.to_local(chr::sys_seconds{chr::seconds{utc}});
  return Date{chr::floor<chr::days>(local - chr::hours{rollover_hour})};
}

UtcRange day_utc_range(Date day, const chr::time_zone& tz, int rollover_hour) {
  check_rollover(rollover_hour);
  const chr::local_seconds start = chr::local_days{day} + chr::hours{rollover_hour};
  return {local_to_utc(start, tz), local_to_utc(start + chr::days{1}, tz)};
}

Date add_days(Date d, int days) {
  return Date{chr::sys_days{d} + chr::days{days}};
}

std::optional<std::pair<int, int>> parse_clock(std::string_view s) {
  const auto colon = s.find(':');
  if (colon == std::string_view::npos || colon == 0 || colon > 2 || s.size() - colon != 3) {
    return std::nullopt;
  }
  auto parse_int = [](std::string_view part, int& out) {
    const auto* end = part.data() + part.size();
    const auto [ptr, ec] = std::from_chars(part.data(), end, out);
    return ec == std::errc{} && ptr == end;
  };
  int h = 0, m = 0;
  if (!parse_int(s.substr(0, colon), h) || !parse_int(s.substr(colon + 1), m)) return std::nullopt;
  if (h < 0 || h > 23 || m < 0 || m > 59) return std::nullopt;
  return std::pair{h, m};
}

UnixSeconds study_day_time(Date day, int hour, int minute, const chr::time_zone& tz,
                           int rollover_hour) {
  check_rollover(rollover_hour);
  const Date calendar_day = hour < rollover_hour ? add_days(day, 1) : day;
  const chr::local_seconds local =
      chr::local_days{calendar_day} + chr::hours{hour} + chr::minutes{minute};
  return local_to_utc(local, tz);
}

std::string format_duration(std::int64_t seconds) {
  seconds = std::max<std::int64_t>(0, seconds);
  if (seconds > 0 && seconds < 60) return std::to_string(seconds) + "s";
  const std::int64_t minutes = seconds / 60;
  const std::int64_t h = minutes / 60;
  const std::int64_t m = minutes % 60;
  return h > 0 ? std::to_string(h) + "h " + std::to_string(m) + "m" : std::to_string(m) + "m";
}

std::string format_clock(UnixSeconds utc, const chr::time_zone& tz) {
  const chr::local_seconds local = tz.to_local(chr::sys_seconds{chr::seconds{utc}});
  const auto since_midnight = local - chr::floor<chr::days>(local);
  const auto h = chr::duration_cast<chr::hours>(since_midnight).count();
  const auto m = chr::duration_cast<chr::minutes>(since_midnight).count() % 60;
  char buf[8];
  std::snprintf(buf, sizeof buf, "%02d:%02d", static_cast<int>(h), static_cast<int>(m));
  return buf;
}

std::string format_short_date(Date d) {
  static constexpr const char* kMonths[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                            "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  return std::string(kMonths[static_cast<unsigned>(d.month()) - 1]) + " " +
         std::to_string(static_cast<unsigned>(d.day())) + ", " +
         std::to_string(static_cast<int>(d.year()));
}

std::string format_date(Date d) {
  char buf[16];
  std::snprintf(buf, sizeof buf, "%04d-%02u-%02u", static_cast<int>(d.year()),
                static_cast<unsigned>(d.month()), static_cast<unsigned>(d.day()));
  return buf;
}

std::optional<Date> parse_date(std::string_view s) {
  if (s.size() != 10 || s[4] != '-' || s[7] != '-') return std::nullopt;
  auto parse_int = [](std::string_view part, int& out) {
    const auto* end = part.data() + part.size();
    const auto [ptr, ec] = std::from_chars(part.data(), end, out);
    return ec == std::errc{} && ptr == end;
  };
  int y = 0, m = 0, d = 0;
  if (!parse_int(s.substr(0, 4), y) || !parse_int(s.substr(5, 2), m) ||
      !parse_int(s.substr(8, 2), d)) {
    return std::nullopt;
  }
  const Date date{chr::year{y}, chr::month{static_cast<unsigned>(m)},
                  chr::day{static_cast<unsigned>(d)}};
  if (!date.ok()) return std::nullopt;
  return date;
}

}  // namespace mattrack
