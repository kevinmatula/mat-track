#include "core/config.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <stdexcept>
#include <variant>

#include "core/file_util.hpp"

namespace mattrack {

namespace {

using Field = std::variant<double Config::*, int Config::*, bool Config::*, WeekStart Config::*>;

struct KeySpec {
  std::string_view name;
  Field field;
  double min = 0;  // numeric keys only
  double max = 0;
  std::string_view comment;
};

// Order here is the order keys are written in.
constexpr std::array kKeys = {
    KeySpec{"focus_minutes", &Config::focus_minutes, 0.1, 600, "length of a focus session"},
    KeySpec{"break_minutes", &Config::break_minutes, 0.1, 600, "short break"},
    KeySpec{"long_break_minutes", &Config::long_break_minutes, 0.1, 600, "long break"},
    KeySpec{"long_break_every", &Config::long_break_every, 1, 100,
            "long break after this many focus sessions"},
    KeySpec{"auto_start_next", &Config::auto_start_next, 0, 0,
            "start the next focus session automatically after a break"},
    KeySpec{"min_log_seconds", &Config::min_log_seconds, 0, 3600,
            "sessions shorter than this are not logged"},
    KeySpec{"max_session_minutes", &Config::max_session_minutes, 1, 1440,
            "a running session auto-stops after this long"},
    KeySpec{"day_rollover_hour", &Config::day_rollover_hour, 0, 12,
            "study day starts at this local hour (0-12)"},
    KeySpec{"week_starts_on", &Config::week_starts_on, 0, 0, "\"monday\" or \"sunday\""},
    KeySpec{"daily_goal_minutes", &Config::daily_goal_minutes, 1, 1440, "daily study goal"},
    KeySpec{"streak_min_minutes", &Config::streak_min_minutes, 1, 1440,
            "minimum minutes for a day to count toward a streak"},
    KeySpec{"sound_enabled", &Config::sound_enabled, 0, 0,
            "repeat a jingle when a session or break ends, until you respond"},
    KeySpec{"raise_on_alarm", &Config::raise_on_alarm, 0, 0,
            "bring the window to the front and focus it when a session or break ends"},
    KeySpec{"invert_colors", &Config::invert_colors, 0, 0, "black on white instead (F2)"},
};

std::string_view trim(std::string_view s) {
  const auto ws = " \t\r\n";
  const auto first = s.find_first_not_of(ws);
  if (first == std::string_view::npos) return {};
  return s.substr(first, s.find_last_not_of(ws) - first + 1);
}

// Drops a trailing `# comment` that is not inside a quoted string.
std::string_view strip_comment(std::string_view line) {
  bool quoted = false;
  for (std::size_t i = 0; i < line.size(); ++i) {
    if (line[i] == '"') quoted = !quoted;
    if (line[i] == '#' && !quoted) return line.substr(0, i);
  }
  return line;
}

std::string lower(std::string_view s) {
  std::string out(s);
  std::transform(out.begin(), out.end(), out.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return out;
}

std::string unquote(std::string_view v) {
  if (v.size() >= 2 && v.front() == '"' && v.back() == '"') v = v.substr(1, v.size() - 2);
  return std::string(v);
}

template <typename T>
bool parse_number(std::string_view v, T& out) {
  const auto* end = v.data() + v.size();
  const auto [ptr, ec] = std::from_chars(v.data(), end, out);
  return ec == std::errc{} && ptr == end;
}

std::string format_number(double v) {
  char buf[32];
  const auto [ptr, ec] = std::to_chars(buf, buf + sizeof buf, v);
  return ec == std::errc{} ? std::string(buf, ptr) : std::to_string(v);
}

class Parser {
 public:
  explicit Parser(std::vector<std::string>* warnings) : warnings_(warnings) {}

  void apply(Config& cfg, const KeySpec& spec, std::string_view raw, int line_no) {
    std::visit([&](auto field) { set(cfg, spec, field, raw, line_no); }, spec.field);
  }

  void warn(int line_no, const std::string& msg) {
    if (warnings_) warnings_->push_back("config line " + std::to_string(line_no) + ": " + msg);
  }

 private:
  template <typename T>
  void set_clamped(Config& cfg, const KeySpec& spec, T Config::*field, T value, int line_no) {
    const T lo = static_cast<T>(spec.min);
    const T hi = static_cast<T>(spec.max);
    if (value < lo || value > hi) {
      const T clamped = std::clamp(value, lo, hi);
      warn(line_no, std::string(spec.name) + " out of range [" + format_number(spec.min) + ", " +
                        format_number(spec.max) + "]; using " + format_number(clamped));
      value = clamped;
    }
    cfg.*field = value;
  }

  void invalid(const KeySpec& spec, std::string_view raw, int line_no, std::string_view want) {
    warn(line_no, "invalid value '" + std::string(raw) + "' for " + std::string(spec.name) +
                      " (expected " + std::string(want) + "); using default");
  }

  void set(Config& cfg, const KeySpec& spec, double Config::*field, std::string_view raw,
           int line_no) {
    double v = 0;
    if (!parse_number(raw, v)) return invalid(spec, raw, line_no, "a number");
    set_clamped(cfg, spec, field, v, line_no);
  }

  void set(Config& cfg, const KeySpec& spec, int Config::*field, std::string_view raw,
           int line_no) {
    int v = 0;
    if (!parse_number(raw, v)) return invalid(spec, raw, line_no, "a whole number");
    set_clamped(cfg, spec, field, v, line_no);
  }

  void set(Config& cfg, const KeySpec& spec, bool Config::*field, std::string_view raw,
           int line_no) {
    if (raw == "true") {
      cfg.*field = true;
    } else if (raw == "false") {
      cfg.*field = false;
    } else {
      invalid(spec, raw, line_no, "true or false");
    }
  }

  void set(Config& cfg, const KeySpec& spec, WeekStart Config::*field, std::string_view raw,
           int line_no) {
    const std::string v = lower(unquote(raw));
    if (v == "monday") {
      cfg.*field = WeekStart::Monday;
    } else if (v == "sunday") {
      cfg.*field = WeekStart::Sunday;
    } else {
      invalid(spec, raw, line_no, "\"monday\" or \"sunday\"");
    }
  }

  std::vector<std::string>* warnings_;
};

std::string value_string(const Config& cfg, const KeySpec& spec) {
  return std::visit(
      [&](auto field) -> std::string {
        const auto& v = cfg.*field;
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, bool>) {
          return v ? "true" : "false";
        } else if constexpr (std::is_same_v<T, WeekStart>) {
          return v == WeekStart::Monday ? "\"monday\"" : "\"sunday\"";
        } else {
          return format_number(static_cast<double>(v));
        }
      },
      spec.field);
}

}  // namespace

Config parse_config(std::string_view text, std::vector<std::string>* warnings) {
  Config cfg;
  Parser parser(warnings);
  std::vector<std::string_view> seen;

  int line_no = 0;
  while (!text.empty()) {
    ++line_no;
    const auto nl = text.find('\n');
    const std::string_view line = trim(strip_comment(text.substr(0, nl)));
    text = nl == std::string_view::npos ? std::string_view{} : text.substr(nl + 1);
    if (line.empty()) continue;

    const auto eq = line.find('=');
    if (eq == std::string_view::npos) {
      parser.warn(line_no, "expected 'key = value'");
      continue;
    }
    const std::string_view key = trim(line.substr(0, eq));
    const std::string_view value = trim(line.substr(eq + 1));

    const auto spec = std::find_if(kKeys.begin(), kKeys.end(),
                                   [&](const KeySpec& k) { return k.name == key; });
    if (spec == kKeys.end()) {
      parser.warn(line_no, "unknown key '" + std::string(key) + "' ignored");
      continue;
    }
    if (std::find(seen.begin(), seen.end(), spec->name) != seen.end()) {
      parser.warn(line_no, "duplicate key '" + std::string(key) + "'; last value wins");
    }
    seen.push_back(spec->name);
    parser.apply(cfg, *spec, value, line_no);
  }
  return cfg;
}

std::string serialize_config(const Config& cfg) {
  std::string out = "# mat-track settings. Read at startup.\n\n";
  for (const auto& spec : kKeys) {
    out += "# ";
    out += spec.comment;
    out += '\n';
    out += spec.name;
    out += " = ";
    out += value_string(cfg, spec);
    out += "\n\n";
  }
  out.pop_back();  // single trailing newline
  return out;
}

Config load_or_create_config(const std::filesystem::path& path,
                             std::vector<std::string>* warnings) {
  const std::optional<std::string> text = read_file(path);
  if (!text) {
    const Config defaults;
    save_config(defaults, path);
    return defaults;
  }
  return parse_config(*text, warnings);
}

void save_config(const Config& config, const std::filesystem::path& path) {
  write_file_atomic(path, serialize_config(config));
}

}  // namespace mattrack
