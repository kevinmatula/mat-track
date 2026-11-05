#pragma once

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "core/models.hpp"

struct sqlite3;

namespace mattrack {

class DbError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

// Owns one SQLite connection. Opening runs any pending schema migrations.
// All methods throw DbError on SQLite failures.
class Db {
 public:
  static constexpr int kSchemaVersion = 1;
  static constexpr std::string_view kDefaultCategory = "General";

  // `path` may be ":memory:" for tests.
  explicit Db(const std::string& path);
  ~Db();

  Db(const Db&) = delete;
  Db& operator=(const Db&) = delete;
  Db(Db&& other) noexcept;
  Db& operator=(Db&& other) noexcept;

  int schema_version() const;

  // ---- categories ----
  std::vector<Category> categories(bool include_archived = true) const;  // by name
  std::optional<Category> category(std::int64_t id) const;
  // Names are trimmed; empty names throw std::invalid_argument, duplicates
  // (case-insensitive) throw DbError.
  std::int64_t add_category(std::string_view name);
  bool rename_category(std::int64_t id, std::string_view name);
  bool set_category_archived(std::int64_t id, bool archived);
  // Refuses (returns false) if the category has sessions; archive instead.
  bool delete_category(std::int64_t id);

  // ---- sessions ----
  // Ignores session.id; returns the new id. Throws std::invalid_argument if
  // end_utc < start_utc or duration_sec < 0.
  std::int64_t insert_session(const Session& session);
  std::optional<Session> session(std::int64_t id) const;
  // Sessions with start_utc in [start, end), ordered by start_utc.
  std::vector<Session> sessions_between(UnixSeconds start, UnixSeconds end) const;
  std::vector<Session> all_sessions() const;
  bool update_session_category(std::int64_t id, std::int64_t category_id);
  bool update_session_note(std::int64_t id, std::string_view note);
  bool delete_session(std::int64_t id);

  // Runs `fn` atomically; rolls back and rethrows if it throws. Uses a
  // savepoint, so transactions may nest.
  template <typename Fn>
  void transaction(Fn&& fn) {
    exec("SAVEPOINT mt_tx");
    try {
      fn();
    } catch (...) {
      exec("ROLLBACK TO mt_tx");
      exec("RELEASE mt_tx");
      throw;
    }
    exec("RELEASE mt_tx");
  }

  // Escape hatch for tests / one-off statements without results.
  void exec(const char* sql);

 private:
  void migrate();

  sqlite3* db_ = nullptr;
};

}  // namespace mattrack
