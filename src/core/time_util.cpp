#include "core/time_util.hpp"

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
