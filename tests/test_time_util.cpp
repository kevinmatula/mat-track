#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include "core/time_util.hpp"

using namespace mattrack;
using namespace std::chrono;

namespace {

// Tests always use an explicit zone, never the machine's local zone.
const time_zone& ny() { return *locate_zone("America/New_York"); }

Date ymd(int y, unsigned m, unsigned d) { return Date{year{y}, month{m}, day{d}}; }

// Reference timestamps below were computed independently with
// `TZ=America/New_York date -d '<local time>' +%s`.

}  // namespace

TEST_CASE("day_key with default rollover (4)", "[time_util]") {
  constexpr int kRollover = 4;

  SECTION("11:50pm counts for that date") {
    CHECK(day_key(1718509800, ny(), kRollover) == ymd(2024, 6, 15));  // 2024-06-15 23:50
  }
  SECTION("12:20am counts for the previous date") {
    CHECK(day_key(1718511600, ny(), kRollover) == ymd(2024, 6, 15));  // 2024-06-16 00:20
  }
  SECTION("3:59:59am counts for the previous date") {
    CHECK(day_key(1718524799, ny(), kRollover) == ymd(2024, 6, 15));  // 2024-06-16 03:59:59
  }
  SECTION("4:00am counts for the new date") {
    CHECK(day_key(1718524800, ny(), kRollover) == ymd(2024, 6, 16));  // 2024-06-16 04:00
  }
}

TEST_CASE("rollover 0 behaves like plain calendar days", "[time_util]") {
  CHECK(day_key(1718509800, ny(), 0) == ymd(2024, 6, 15));  // 23:50
  CHECK(day_key(1718511600, ny(), 0) == ymd(2024, 6, 16));  // 00:20 next day
  CHECK(day_key(1718524799, ny(), 0) == ymd(2024, 6, 16));  // 03:59:59

  const UtcRange r = day_utc_range(ymd(2024, 6, 16), ny(), 0);
  CHECK(r.start == 1718510400);  // 2024-06-16 00:00 EDT
  CHECK(r.end - r.start == 24 * 3600);
}

TEST_CASE("day_utc_range on a normal day", "[time_util]") {
  const UtcRange r = day_utc_range(ymd(2024, 6, 15), ny(), 4);
  CHECK(r.start == 1718438400);  // 2024-06-15 04:00 EDT
  CHECK(r.end == 1718524800);    // 2024-06-16 04:00 EDT
  CHECK(r.contains(1718511600));   // 00:20 on the 16th
  CHECK_FALSE(r.contains(r.end));  // half-open
}

TEST_CASE("DST spring forward (2024-03-10, 02:00 -> 03:00)", "[time_util][dst]") {
  SECTION("rollover 4: the study day spanning the gap is 23 hours") {
    const UtcRange r = day_utc_range(ymd(2024, 3, 9), ny(), 4);
    CHECK(r.start == 1709974800);  // 2024-03-09 04:00 EST
    CHECK(r.end == 1710057600);    // 2024-03-10 04:00 EDT
    CHECK(r.end - r.start == 23 * 3600);
    CHECK(day_utc_range(ymd(2024, 3, 10), ny(), 4).end -
              day_utc_range(ymd(2024, 3, 10), ny(), 4).start ==
          24 * 3600);
  }
  SECTION("rollover 4: times just after the jump still belong to the previous day") {
    CHECK(day_key(1710053999, ny(), 4) == ymd(2024, 3, 9));  // 01:59:59 EST
    CHECK(day_key(1710055800, ny(), 4) == ymd(2024, 3, 9));  // 03:30 EDT
    CHECK(day_key(1710057600, ny(), 4) == ymd(2024, 3, 10));  // 04:00 EDT
  }
  SECTION("rollover 0: the calendar day is 23 hours") {
    const UtcRange r = day_utc_range(ymd(2024, 3, 10), ny(), 0);
    CHECK(r.start == 1710046800);  // 2024-03-10 00:00 EST
    CHECK(r.end == 1710129600);    // 2024-03-11 00:00 EDT
    CHECK(r.end - r.start == 23 * 3600);
  }
  SECTION("rollover inside the gap (2): boundary snaps to the transition") {
    // 02:00 local does not exist; the 10th starts at 03:00 EDT (07:00 UTC).
    const UtcRange r = day_utc_range(ymd(2024, 3, 10), ny(), 2);
    CHECK(r.start == 1710054000);
    CHECK(day_key(1710053999, ny(), 2) == ymd(2024, 3, 9));
    CHECK(day_key(1710054000, ny(), 2) == ymd(2024, 3, 10));
  }
}

TEST_CASE("DST fall back (2024-11-03, 02:00 -> 01:00)", "[time_util][dst]") {
  SECTION("rollover 4: the study day spanning the overlap is 25 hours") {
    const UtcRange r = day_utc_range(ymd(2024, 11, 2), ny(), 4);
    CHECK(r.start == 1730534400);  // 2024-11-02 04:00 EDT
    CHECK(r.end == 1730624400);    // 2024-11-03 04:00 EST
    CHECK(r.end - r.start == 25 * 3600);
  }
  SECTION("rollover 4: both occurrences of 01:30 belong to the previous day") {
    CHECK(day_key(1730611800, ny(), 4) == ymd(2024, 11, 2));  // 01:30 EDT
    CHECK(day_key(1730615400, ny(), 4) == ymd(2024, 11, 2));  // 01:30 EST
  }
  SECTION("rollover 0: the calendar day is 25 hours") {
    const UtcRange r = day_utc_range(ymd(2024, 11, 3), ny(), 0);
    CHECK(r.start == 1730606400);  // 2024-11-03 00:00 EDT
    CHECK(r.end == 1730696400);    // 2024-11-04 00:00 EST
    CHECK(r.end - r.start == 25 * 3600);
  }
  SECTION("rollover inside the overlap (1): boundary is the first 01:00") {
    const UtcRange r = day_utc_range(ymd(2024, 11, 3), ny(), 1);
    CHECK(r.start == 1730610000);  // 01:00 EDT
    CHECK(day_key(1730611800, ny(), 1) == ymd(2024, 11, 3));  // 01:30 EDT
    CHECK(day_key(1730615400, ny(), 1) == ymd(2024, 11, 3));  // 01:30 EST
  }
}

TEST_CASE("day_key and day_utc_range agree everywhere", "[time_util][property]") {
  // Sweep every 10 minutes across both 2024 US/EU DST transitions, in zones
  // with different offsets and DST rules, for every allowed rollover hour.
  const auto zone_name = GENERATE("America/New_York", "Europe/London",
                                  "Australia/Sydney", "Asia/Kolkata", "UTC");
  const time_zone& tz = *locate_zone(zone_name);
  const int rollover = GENERATE(range(kMinRolloverHour, kMaxRolloverHour + 1));
  INFO(zone_name << " rollover=" << rollover);

  const std::pair<UnixSeconds, UnixSeconds> windows[] = {
      {1709856000, 1712534400},  // 2024-03-08 .. 2024-04-08 (US + EU + AU)
      {1727740800, 1731024000},  // 2024-10-01 .. 2024-11-08
  };
  for (const auto& [from, to] : windows) {
    for (UnixSeconds t = from; t < to; t += 600) {
      const Date d = day_key(t, tz, rollover);
      const UtcRange r = day_utc_range(d, tz, rollover);
      if (!r.contains(t)) {
        FAIL("t=" << t << " day=" << format_date(d) << " range=[" << r.start << ","
                  << r.end << ")");
      }
      // Ranges of consecutive days tile time with no gaps or overlaps.
      REQUIRE(day_utc_range(add_days(d, 1), tz, rollover).start == r.end);
    }
  }
}

TEST_CASE("rollover hour is validated", "[time_util]") {
  CHECK_THROWS_AS(day_key(0, ny(), -1), std::invalid_argument);
  CHECK_THROWS_AS(day_key(0, ny(), 13), std::invalid_argument);
  CHECK_THROWS_AS(day_utc_range(ymd(2024, 1, 1), ny(), 13), std::invalid_argument);
  CHECK_NOTHROW(day_key(0, ny(), 12));
}

TEST_CASE("date helpers", "[time_util]") {
  CHECK(format_date(ymd(2024, 3, 9)) == "2024-03-09");
  CHECK(parse_date("2024-03-09") == ymd(2024, 3, 9));
  CHECK(parse_date("2024-02-29") == ymd(2024, 2, 29));
  CHECK_FALSE(parse_date("2023-02-29"));
  CHECK_FALSE(parse_date("2024-13-01"));
  CHECK_FALSE(parse_date("2024-3-9"));
  CHECK_FALSE(parse_date("2024/03/09"));
  CHECK_FALSE(parse_date("abcd-ef-gh"));
  CHECK(add_days(ymd(2024, 2, 28), 1) == ymd(2024, 2, 29));
  CHECK(add_days(ymd(2024, 12, 31), 1) == ymd(2025, 1, 1));
  CHECK(add_days(ymd(2024, 3, 1), -1) == ymd(2024, 2, 29));
}
