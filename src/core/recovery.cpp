#include "core/recovery.hpp"

#include <algorithm>
#include <charconv>

#include "core/file_util.hpp"

namespace mattrack {

namespace {

// Finds `"key": <integer>` in a flat JSON object.
std::optional<std::int64_t> find_int(std::string_view text, std::string_view key) {
  const std::string quoted = "\"" + std::string(key) + "\"";
  auto pos = text.find(quoted);
  if (pos == std::string_view::npos) return std::nullopt;
  pos = text.find_first_not_of(" \t\r\n", pos + quoted.size());
  if (pos == std::string_view::npos || text[pos] != ':') return std::nullopt;
  pos = text.find_first_not_of(" \t\r\n", pos + 1);
  if (pos == std::string_view::npos) return std::nullopt;

  std::int64_t value = 0;
  const auto [ptr, ec] = std::from_chars(text.data() + pos, text.data() + text.size(), value);
  if (ec != std::errc{}) return std::nullopt;
  // The number must be followed by `,` or `}` (rejects 12.5, 12abc).
  const auto next =
      text.find_first_not_of(" \t\r\n", static_cast<std::size_t>(ptr - text.data()));
  if (next == std::string_view::npos || (text[next] != ',' && text[next] != '}')) {
    return std::nullopt;
  }
  return value;
}

}  // namespace

std::string serialize_recovery(const RecoveryState& s) {
  return "{\n"
         "  \"start_utc\": " + std::to_string(s.start_utc) + ",\n" +
         "  \"category_id\": " + std::to_string(s.category_id) + ",\n" +
         "  \"focused_seconds\": " + std::to_string(s.focused_seconds) + ",\n" +
         "  \"updated_utc\": " + std::to_string(s.updated_utc) + "\n" +
         "}\n";
}

std::optional<RecoveryState> parse_recovery(std::string_view text) {
  const auto start = find_int(text, "start_utc");
  const auto category = find_int(text, "category_id");
  const auto focused = find_int(text, "focused_seconds");
  const auto updated = find_int(text, "updated_utc");
  if (!start || !category || !focused || !updated || *focused < 0) return std::nullopt;
  return RecoveryState{*start, *category, *focused, *updated};
}

Session recovered_session(const RecoveryState& state) {
  Session s;
  s.start_utc = state.start_utc;
  s.end_utc = std::max(state.updated_utc, state.start_utc);
  s.duration_sec = state.focused_seconds;
  s.category_id = state.category_id;
  s.completed = false;
  return s;
}

void save_recovery(const RecoveryState& state, const std::filesystem::path& path) {
  write_file_atomic(path, serialize_recovery(state));
}

void remove_recovery(const std::filesystem::path& path) {
  std::error_code ec;
  std::filesystem::remove(path, ec);
}

}  // namespace mattrack
