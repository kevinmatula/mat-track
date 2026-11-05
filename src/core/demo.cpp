#include "core/demo.hpp"

#include <algorithm>
#include <array>
#include <random>
#include <string_view>

#include "core/time_util.hpp"

namespace mattrack {

namespace chr = std::chrono;

namespace {

std::int64_t find_or_add_category(Db& db, std::string_view name) {
  for (const auto& c : db.categories()) {
    if (c.name == name) return c.id;
  }
  return db.add_category(name);
}

}  // namespace

int seed_demo(Db& db, UnixSeconds now, const chr::time_zone& tz, int rollover_hour,
              unsigned seed) {
  std::mt19937 rng(seed);
  auto chance = [&](double p) { return std::uniform_real_distribution<>(0, 1)(rng) < p; };
  auto between = [&](int lo, int hi) { return std::uniform_int_distribution<>(lo, hi)(rng); };

  int inserted = 0;
  db.transaction([&] {
    // Weighted so the per-category breakdown is uneven.
    const std::array<std::int64_t, 4> cats = {
        find_or_add_category(db, "Math"), find_or_add_category(db, "Physics"),
        find_or_add_category(db, "Reading"), find_or_add_category(db, "Coding")};
    std::discrete_distribution<> pick_cat({4, 3, 2, 3});

    const Date today = day_key(now, tz, rollover_hour);
    int vacation_left = 0;
    for (int back = kDemoDays - 1; back >= 0; --back) {
      const Date d = add_days(today, -back);
      if (vacation_left > 0) {
        --vacation_left;
        continue;
      }
      if (chance(0.02)) vacation_left = between(2, 5);

      const chr::weekday wd{chr::sys_days{d}};
      const bool weekend = wd == chr::Saturday || wd == chr::Sunday;
      if (!chance(weekend ? 0.55 : 0.8)) continue;

      // Some light days, some long ones; occasionally a big push.
      int sessions = between(1, 3);
      if (chance(0.15)) sessions += between(2, 3);

      // Minutes after local midnight of the study day; may run past 24h
      // (after midnight), which still belongs to this study day.
      int clock = between(8, 14) * 60 + between(0, 59);
      if (chance(0.1)) clock = 21 * 60 + between(0, 90);  // a night owl day

      for (int i = 0; i < sessions; ++i) {
        int minutes = 50;  // a completed default-length pomodoro
        bool completed = true;
        if (chance(0.25)) {
          minutes = between(5, 45);  // stopped early
          completed = false;
        } else if (chance(0.05)) {
          minutes = between(80, 130);  // ran a long custom session
        }

        const chr::local_seconds local = chr::local_days{d} + chr::minutes{clock};
        const UnixSeconds start =
            tz.to_sys(local, chr::choose::earliest).time_since_epoch().count();
        const UnixSeconds end = start + minutes * 60;
        if (end > now) break;

        Session s;
        s.start_utc = start;
        s.end_utc = end;
        s.duration_sec = minutes * 60 + (completed ? 0 : between(0, 59));
        s.category_id = cats[static_cast<std::size_t>(pick_cat(rng))];
        s.completed = completed;
        db.insert_session(s);
        ++inserted;

        clock += minutes + between(5, 40);  // a break before the next one
      }
    }
  });
  return inserted;
}

}  // namespace mattrack
