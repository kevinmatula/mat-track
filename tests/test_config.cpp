#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>

#include "core/config.hpp"

using namespace mattrack;
using Catch::Matchers::ContainsSubstring;
namespace fs = std::filesystem;

namespace {

struct TempDir {
  fs::path path;
  TempDir() {
    std::random_device rd;
    path = fs::temp_directory_path() / ("mat-track-test-" + std::to_string(rd()));
    fs::create_directories(path);
  }
  ~TempDir() {
    std::error_code ec;
    fs::remove_all(path, ec);
  }
};

std::string read_file(const fs::path& p) {
  std::ifstream in(p);
  std::ostringstream s;
  s << in.rdbuf();
  return s.str();
}

}  // namespace

TEST_CASE("defaults match the spec", "[config]") {
  const Config c;
  CHECK(c.focus_minutes == 50);
  CHECK(c.break_minutes == 10);
  CHECK(c.long_break_minutes == 30);
  CHECK(c.long_break_every == 4);
  CHECK_FALSE(c.auto_start_next);
  CHECK(c.min_log_seconds == 60);
  CHECK(c.max_session_minutes == 240);
  CHECK(c.day_rollover_hour == 4);
  CHECK(c.week_starts_on == WeekStart::Monday);
  CHECK(c.daily_goal_minutes == 120);
  CHECK(c.streak_min_minutes == 30);
  CHECK(c.sound_enabled);
  CHECK(c.raise_on_alarm);
  CHECK_FALSE(c.invert_colors);
}

TEST_CASE("empty text gives defaults with no warnings", "[config]") {
  std::vector<std::string> warnings;
  CHECK(parse_config("", &warnings) == Config{});
  CHECK(parse_config("\n# just a comment\n   \n", &warnings) == Config{});
  CHECK(warnings.empty());
}

TEST_CASE("parses the spec's example file", "[config]") {
  const std::string text = R"(
focus_minutes = 25
break_minutes = 5
long_break_minutes = 15
long_break_every = 3
auto_start_next = true
min_log_seconds = 30

day_rollover_hour = 0
week_starts_on = "sunday"      # or "monday"

daily_goal_minutes = 90
streak_min_minutes = 15

sound_enabled = false
invert_colors = true
max_session_minutes = 120
)";
  std::vector<std::string> warnings;
  const Config c = parse_config(text, &warnings);
  CHECK(warnings.empty());
  CHECK(c.focus_minutes == 25);
  CHECK(c.break_minutes == 5);
  CHECK(c.long_break_minutes == 15);
  CHECK(c.long_break_every == 3);
  CHECK(c.auto_start_next);
  CHECK(c.min_log_seconds == 30);
  CHECK(c.day_rollover_hour == 0);
  CHECK(c.week_starts_on == WeekStart::Sunday);
  CHECK(c.daily_goal_minutes == 90);
  CHECK(c.streak_min_minutes == 15);
  CHECK_FALSE(c.sound_enabled);
  CHECK(c.invert_colors);
  CHECK(c.max_session_minutes == 120);
}

TEST_CASE("fractional durations are allowed for quick testing", "[config]") {
  const Config c = parse_config("focus_minutes = 0.5\nbreak_minutes = 0.25\n");
  CHECK(c.focus_minutes == 0.5);
  CHECK(c.break_minutes == 0.25);
}

TEST_CASE("missing keys keep defaults; whitespace is flexible", "[config]") {
  const Config c = parse_config("  daily_goal_minutes=200  \r\n\tsound_enabled   =   false");
  Config expected;
  expected.daily_goal_minutes = 200;
  expected.sound_enabled = false;
  CHECK(c == expected);
}

TEST_CASE("unknown keys and malformed lines are ignored with a warning", "[config]") {
  std::vector<std::string> warnings;
  const Config c = parse_config("theme = \"neon\"\nthis is not a pair\nfocus_minutes = 40\n",
                                &warnings);
  CHECK(c.focus_minutes == 40);
  REQUIRE(warnings.size() == 2);
  CHECK_THAT(warnings[0], ContainsSubstring("line 1") && ContainsSubstring("theme"));
  CHECK_THAT(warnings[1], ContainsSubstring("line 2"));
}

TEST_CASE("invalid values fall back to the default with a warning", "[config]") {
  std::vector<std::string> warnings;
  const Config c = parse_config(
      "focus_minutes = fifty\n"
      "day_rollover_hour = 4.5\n"  // must be a whole number
      "sound_enabled = yes\n"
      "week_starts_on = \"friday\"\n"
      "long_break_every = \n",
      &warnings);
  CHECK(c == Config{});
  CHECK(warnings.size() == 5);
}

TEST_CASE("out-of-range numbers are clamped with a warning", "[config]") {
  std::vector<std::string> warnings;
  const Config c = parse_config(
      "day_rollover_hour = 13\n"
      "focus_minutes = 0\n"
      "long_break_every = -2\n"
      "daily_goal_minutes = 5000\n",
      &warnings);
  CHECK(c.day_rollover_hour == 12);
  CHECK(c.focus_minutes == 0.1);
  CHECK(c.long_break_every == 1);
  CHECK(c.daily_goal_minutes == 1440);
  CHECK(warnings.size() == 4);
}

TEST_CASE("duplicate keys: last one wins, with a warning", "[config]") {
  std::vector<std::string> warnings;
  const Config c = parse_config("focus_minutes = 30\nfocus_minutes = 45\n", &warnings);
  CHECK(c.focus_minutes == 45);
  CHECK(warnings.size() == 1);
}

TEST_CASE("week_starts_on accepts bare and differently-cased values", "[config]") {
  CHECK(parse_config("week_starts_on = sunday").week_starts_on == WeekStart::Sunday);
  CHECK(parse_config("week_starts_on = \"Sunday\"").week_starts_on == WeekStart::Sunday);
}

TEST_CASE("serialize/parse roundtrip", "[config]") {
  Config c;
  c.focus_minutes = 0.5;
  c.long_break_every = 7;
  c.auto_start_next = true;
  c.week_starts_on = WeekStart::Sunday;
  c.invert_colors = true;
  c.raise_on_alarm = false;
  c.day_rollover_hour = 0;

  std::vector<std::string> warnings;
  CHECK(parse_config(serialize_config(c), &warnings) == c);
  CHECK(warnings.empty());
  CHECK(parse_config(serialize_config(Config{})) == Config{});

  const std::string text = serialize_config(Config{});
  CHECK_THAT(text, ContainsSubstring("focus_minutes = 50\n"));
  CHECK_THAT(text, ContainsSubstring("week_starts_on = \"monday\"\n"));
  CHECK_THAT(text, ContainsSubstring("sound_enabled = true\n"));
}

TEST_CASE("load_or_create_config writes defaults on first run", "[config]") {
  TempDir dir;
  const fs::path path = dir.path / "config.toml";

  CHECK(load_or_create_config(path) == Config{});
  REQUIRE(fs::exists(path));
  CHECK(read_file(path) == serialize_config(Config{}));

  SECTION("an existing file is read, not overwritten") {
    { std::ofstream(path) << "focus_minutes = 20\n"; }
    CHECK(load_or_create_config(path).focus_minutes == 20);
    CHECK(read_file(path) == "focus_minutes = 20\n");
  }
}

TEST_CASE("save_config replaces the file and leaves no temp file", "[config]") {
  TempDir dir;
  const fs::path path = dir.path / "config.toml";
  Config c;
  c.invert_colors = true;
  save_config(c, path);
  c.invert_colors = false;
  save_config(c, path);

  CHECK(load_or_create_config(path) == c);
  CHECK_FALSE(fs::exists(dir.path / "config.toml.tmp"));
  CHECK_THROWS_AS(save_config(c, dir.path / "no-such-dir" / "config.toml"), std::runtime_error);
}
