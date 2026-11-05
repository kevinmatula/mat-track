#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace mattrack {

enum class WeekStart { Monday, Sunday };

// User settings. Defaults here are the defaults written on first run.
// The *_minutes durations accept fractions (e.g. 0.5) so short sessions can
// be tested by hand.
struct Config {
  double focus_minutes = 50;
  double break_minutes = 10;
  double long_break_minutes = 30;
  int long_break_every = 4;
  bool auto_start_next = false;
  int min_log_seconds = 60;
  double max_session_minutes = 240;

  int day_rollover_hour = 4;
  WeekStart week_starts_on = WeekStart::Monday;

  int daily_goal_minutes = 120;
  int streak_min_minutes = 30;

  bool sound_enabled = true;
  bool invert_colors = false;

  bool operator==(const Config&) const = default;
};

// Parses the simple TOML subset used by config.toml: one `key = value` per
// line, `#` comments, values that are booleans, numbers or "strings".
// Never fails: unknown keys are ignored, missing keys keep their defaults,
// invalid values fall back to the default and out-of-range numbers are
// clamped. Each such issue is appended to `warnings` if given.
Config parse_config(std::string_view text, std::vector<std::string>* warnings = nullptr);

// Writes every key, with a short comment per key. Round-trips through
// parse_config(). Note that user comments in an existing file are not kept.
std::string serialize_config(const Config& config);

// Reads `path`. If it does not exist, writes the defaults there first.
// Throws std::runtime_error if the file cannot be read or created.
Config load_or_create_config(const std::filesystem::path& path,
                             std::vector<std::string>* warnings = nullptr);

// Writes atomically (temp file + rename). Throws std::runtime_error on failure.
void save_config(const Config& config, const std::filesystem::path& path);

}  // namespace mattrack
