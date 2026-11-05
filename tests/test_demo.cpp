#include <catch2/catch_test_macros.hpp>

#include "core/demo.hpp"
#include "core/stats.hpp"

using namespace mattrack;
using namespace std::chrono;

namespace {
const time_zone& ny() { return *locate_zone("America/New_York"); }
constexpr UnixSeconds kNow = 1793800000;  // 2026-11-04, mid-afternoon in New York
}  // namespace

TEST_CASE("seed_demo fills about six months of plausible sessions", "[demo]") {
  Db db(":memory:");
  const int n = seed_demo(db, kNow, ny(), 4);
  const auto sessions = db.all_sessions();
  CHECK(n == static_cast<int>(sessions.size()));
  CHECK(n > 150);

  const Date today = day_key(kNow, ny(), 4);
  const DayTotals days = day_totals(sessions, ny(), 4);
  CHECK(days.size() > 90);
  CHECK(days.begin()->first >= add_days(today, -(kDemoDays - 1)));
  CHECK(days.rbegin()->first <= today);

  for (const auto& s : sessions) {
    CHECK(s.end_utc <= kNow);
    CHECK(s.end_utc >= s.start_utc);
    CHECK(s.duration_sec > 0);
    CHECK(db.category(s.category_id).has_value());
  }
  // Demo categories exist next to General.
  CHECK(db.categories().size() == 5);
}

TEST_CASE("seed_demo is deterministic and reuses existing categories", "[demo]") {
  Db a(":memory:");
  Db b(":memory:");
  b.add_category("Math");  // already there: must not be duplicated
  seed_demo(a, kNow, ny(), 4, 7);
  seed_demo(b, kNow, ny(), 4, 7);
  CHECK(b.categories().size() == 5);

  auto strip = [](std::vector<Session> v) {
    for (auto& s : v) s.id = s.category_id = 0;  // ids may differ
    return v;
  };
  CHECK(strip(a.all_sessions()) == strip(b.all_sessions()));
}
