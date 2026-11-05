#include "core/stats.hpp"

#include <algorithm>

namespace mattrack {

namespace chr = std::chrono;

namespace {

bool matches(const Session& s, CategoryFilter filter) {
  return !filter || s.category_id == *filter;
}

void add_to(std::vector<CategoryTotal>& totals, std::int64_t category_id, std::int64_t seconds,
            int sessions) {
  auto it = std::find_if(totals.begin(), totals.end(),
                         [&](const CategoryTotal& c) { return c.category_id == category_id; });
  if (it == totals.end()) {
    totals.push_back({category_id, seconds, sessions});
  } else {
    it->seconds += seconds;
    it->sessions += sessions;
  }
}

void sort_by_time(std::vector<CategoryTotal>& totals) {
  std::sort(totals.begin(), totals.end(), [](const CategoryTotal& a, const CategoryTotal& b) {
    return a.seconds != b.seconds ? a.seconds > b.seconds : a.category_id < b.category_id;
  });
}

}  // namespace

DayTotals day_totals(const std::vector<Session>& sessions, const chr::time_zone& tz,
                     int rollover_hour, CategoryFilter filter) {
  DayTotals days;
  for (const auto& s : sessions) {
    if (!matches(s, filter)) continue;
    DayTotal& d = days[day_key(s.start_utc, tz, rollover_hour)];
    d.seconds += s.duration_sec;
    d.sessions += 1;
    add_to(d.by_category, s.category_id, s.duration_sec, 1);
  }
  for (auto& [_, d] : days) sort_by_time(d.by_category);
  return days;
}

DayTotal total_between(const DayTotals& days, Date first, Date last) {
  DayTotal out;
  for (auto it = days.lower_bound(first); it != days.end() && it->first <= last; ++it) {
    out.seconds += it->second.seconds;
    out.sessions += it->second.sessions;
    for (const auto& c : it->second.by_category) {
      add_to(out.by_category, c.category_id, c.seconds, c.sessions);
    }
  }
  sort_by_time(out.by_category);
  return out;
}

std::vector<CategoryTotal> category_totals(const std::vector<Session>& sessions,
                                           CategoryFilter filter) {
  std::vector<CategoryTotal> out;
  for (const auto& s : sessions) {
    if (matches(s, filter)) add_to(out, s.category_id, s.duration_sec, 1);
  }
  sort_by_time(out);
  return out;
}

Date week_start(Date d, WeekStart starts_on) {
  const chr::weekday wd{chr::sys_days{d}};
  // iso_encoding: Mon=1..Sun=7; c_encoding: Sun=0..Sat=6.
  const unsigned offset =
      starts_on == WeekStart::Monday ? wd.iso_encoding() - 1 : wd.c_encoding();
  return add_days(d, -static_cast<int>(offset));
}

DateSpan week_of(Date d, WeekStart starts_on) {
  const Date first = week_start(d, starts_on);
  return {first, add_days(first, 6)};
}

DateSpan month_of(Date d) {
  return {d.year() / d.month() / chr::day{1},
          Date{d.year() / d.month() / chr::last}};
}

}  // namespace mattrack
