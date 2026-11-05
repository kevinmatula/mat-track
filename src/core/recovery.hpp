#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include "core/models.hpp"

// Crash safety: while a session is active the app keeps a small JSON file
// (active_session.json) up to date. If the app dies, the next start finds it
// and offers to save the session.
namespace mattrack {

struct RecoveryState {
  UnixSeconds start_utc = 0;
  std::int64_t category_id = 0;
  std::int64_t focused_seconds = 0;
  UnixSeconds updated_utc = 0;  // wall time of the last write

  bool operator==(const RecoveryState&) const = default;
};

std::string serialize_recovery(const RecoveryState& state);
// nullopt if the text is not a recovery file this build wrote.
std::optional<RecoveryState> parse_recovery(std::string_view text);

// The session to log for an interrupted run: not completed, ending at the
// last write (never before its start).
Session recovered_session(const RecoveryState& state);

// Throws std::runtime_error on I/O failure.
void save_recovery(const RecoveryState& state, const std::filesystem::path& path);
void remove_recovery(const std::filesystem::path& path);

}  // namespace mattrack
