#pragma once

#include <cstdint>
#include <string>

namespace mattrack {

// Unix seconds, always UTC.
using UnixSeconds = std::int64_t;

struct Category {
  std::int64_t id = 0;
  std::string name;
  bool archived = false;

  bool operator==(const Category&) const = default;
};

struct Session {
  std::int64_t id = 0;          // 0 until written to the DB
  UnixSeconds start_utc = 0;
  UnixSeconds end_utc = 0;
  std::int64_t duration_sec = 0;  // focused time; excludes pauses/suspend
  std::int64_t category_id = 0;
  std::string note;
  bool completed = false;  // timer ran to its target length

  bool operator==(const Session&) const = default;
};

}  // namespace mattrack
