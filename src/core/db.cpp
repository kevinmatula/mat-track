#include "core/db.hpp"

#include <sqlite3.h>

#include <utility>

#include "core/time_util.hpp"

namespace mattrack {

namespace {

[[noreturn]] void fail(sqlite3* db, const std::string& what) {
  throw DbError(what + ": " + (db ? sqlite3_errmsg(db) : "out of memory"));
}

// Minimal RAII prepared statement.
class Stmt {
 public:
  Stmt(sqlite3* db, const char* sql) : db_(db) {
    if (sqlite3_prepare_v2(db, sql, -1, &stmt_, nullptr) != SQLITE_OK) {
      fail(db, std::string("prepare failed (") + sql + ")");
    }
  }
  ~Stmt() { sqlite3_finalize(stmt_); }
  Stmt(const Stmt&) = delete;
  Stmt& operator=(const Stmt&) = delete;

  Stmt& bind(int idx, std::int64_t v) {
    check(sqlite3_bind_int64(stmt_, idx, v));
    return *this;
  }
  Stmt& bind(int idx, std::string_view v) {
    check(sqlite3_bind_text(stmt_, idx, v.data(), static_cast<int>(v.size()),
                            SQLITE_TRANSIENT));
    return *this;
  }

  // Returns true while a row is available.
  bool step() {
    const int rc = sqlite3_step(stmt_);
    if (rc == SQLITE_ROW) return true;
    if (rc == SQLITE_DONE) return false;
    fail(db_, "step failed");
  }

  // Executes a statement that returns no rows.
  void run() {
    while (step()) {
    }
  }

  std::int64_t int_col(int col) const { return sqlite3_column_int64(stmt_, col); }
  std::string text_col(int col) const {
    const auto* p = sqlite3_column_text(stmt_, col);
    return p ? std::string(reinterpret_cast<const char*>(p),
                           static_cast<std::size_t>(sqlite3_column_bytes(stmt_, col)))
             : std::string();
  }

 private:
  void check(int rc) {
    if (rc != SQLITE_OK) fail(db_, "bind failed");
  }

  sqlite3* db_;
  sqlite3_stmt* stmt_ = nullptr;
};

std::string trim_name(std::string_view name) {
  const auto ws = " \t\r\n";
  const auto first = name.find_first_not_of(ws);
  if (first == std::string_view::npos) {
    throw std::invalid_argument("category name must not be empty");
  }
  const auto last = name.find_last_not_of(ws);
  return std::string(name.substr(first, last - first + 1));
}

constexpr const char* kSessionColumns =
    "id, start_utc, end_utc, duration_sec, category_id, note, completed";

Session read_session(const Stmt& s) {
  return Session{
      .id = s.int_col(0),
      .start_utc = s.int_col(1),
      .end_utc = s.int_col(2),
      .duration_sec = s.int_col(3),
      .category_id = s.int_col(4),
      .note = s.text_col(5),
      .completed = s.int_col(6) != 0,
  };
}

Category read_category(const Stmt& s) {
  return Category{.id = s.int_col(0), .name = s.text_col(1), .archived = s.int_col(2) != 0};
}

// Migration N brings the schema from user_version N-1 to N.
constexpr const char* kMigrations[] = {
    // v1
    R"sql(
      CREATE TABLE categories (
        id        INTEGER PRIMARY KEY,
        name      TEXT NOT NULL UNIQUE COLLATE NOCASE,
        archived  INTEGER NOT NULL DEFAULT 0
      );
      CREATE TABLE sessions (
        id           INTEGER PRIMARY KEY,
        start_utc    INTEGER NOT NULL,
        end_utc      INTEGER NOT NULL,
        duration_sec INTEGER NOT NULL,
        category_id  INTEGER NOT NULL REFERENCES categories(id),
        note         TEXT NOT NULL DEFAULT '',
        completed    INTEGER NOT NULL DEFAULT 0
      );
      CREATE INDEX idx_sessions_start ON sessions(start_utc);
      CREATE INDEX idx_sessions_cat   ON sessions(category_id);
      INSERT INTO categories(name) VALUES ('General');
    )sql",
    // v2: the daily goal over time, so changing it never regrades the past.
    R"sql(
      CREATE TABLE goal_changes (
        from_day  TEXT PRIMARY KEY,   -- 'YYYY-MM-DD' study day
        minutes   INTEGER NOT NULL CHECK (minutes > 0)
      );
    )sql",
};
static_assert(std::size(kMigrations) == Db::kSchemaVersion);

}  // namespace

Db::Db(const std::string& path) {
  const int rc = sqlite3_open_v2(path.c_str(), &db_,
                                 SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);
  if (rc != SQLITE_OK) {
    const std::string msg = "cannot open database '" + path + "': " +
                            (db_ ? sqlite3_errmsg(db_) : sqlite3_errstr(rc));
    sqlite3_close(db_);
    db_ = nullptr;
    throw DbError(msg);
  }
  try {
    sqlite3_busy_timeout(db_, 2000);
    exec("PRAGMA foreign_keys = ON");
    // In-memory databases report "memory" and ignore this; that's fine.
    Stmt(db_, "PRAGMA journal_mode = WAL").run();
    migrate();
  } catch (...) {
    sqlite3_close(db_);
    db_ = nullptr;
    throw;
  }
}

Db::~Db() { sqlite3_close(db_); }

Db::Db(Db&& other) noexcept : db_(std::exchange(other.db_, nullptr)) {}

Db& Db::operator=(Db&& other) noexcept {
  if (this != &other) {
    sqlite3_close(db_);
    db_ = std::exchange(other.db_, nullptr);
  }
  return *this;
}

void Db::exec(const char* sql) {
  char* err = nullptr;
  if (sqlite3_exec(db_, sql, nullptr, nullptr, &err) != SQLITE_OK) {
    std::string msg = err ? err : "unknown error";
    sqlite3_free(err);
    throw DbError("exec failed: " + msg);
  }
}

int Db::schema_version() const {
  Stmt s(db_, "PRAGMA user_version");
  s.step();
  return static_cast<int>(s.int_col(0));
}

void Db::migrate() {
  const int current = schema_version();
  if (current > kSchemaVersion) {
    throw DbError("database schema version " + std::to_string(current) +
                  " is newer than this build supports (" +
                  std::to_string(kSchemaVersion) + ")");
  }
  for (int v = current + 1; v <= kSchemaVersion; ++v) {
    transaction([&] {
      exec(kMigrations[v - 1]);
      exec(("PRAGMA user_version = " + std::to_string(v)).c_str());
    });
  }
}

// ---- categories -------------------------------------------------------------

std::vector<Category> Db::categories(bool include_archived) const {
  Stmt s(db_, include_archived
                  ? "SELECT id, name, archived FROM categories ORDER BY name"
                  : "SELECT id, name, archived FROM categories WHERE archived = 0 "
                    "ORDER BY name");
  std::vector<Category> out;
  while (s.step()) out.push_back(read_category(s));
  return out;
}

std::optional<Category> Db::category(std::int64_t id) const {
  Stmt s(db_, "SELECT id, name, archived FROM categories WHERE id = ?");
  s.bind(1, id);
  if (!s.step()) return std::nullopt;
  return read_category(s);
}

std::int64_t Db::add_category(std::string_view name) {
  const std::string trimmed = trim_name(name);
  Stmt(db_, "INSERT INTO categories(name) VALUES (?)").bind(1, trimmed).run();
  return sqlite3_last_insert_rowid(db_);
}

bool Db::rename_category(std::int64_t id, std::string_view name) {
  const std::string trimmed = trim_name(name);
  Stmt(db_, "UPDATE categories SET name = ? WHERE id = ?").bind(1, trimmed).bind(2, id).run();
  return sqlite3_changes(db_) > 0;
}

bool Db::set_category_archived(std::int64_t id, bool archived) {
  Stmt(db_, "UPDATE categories SET archived = ? WHERE id = ?")
      .bind(1, std::int64_t{archived})
      .bind(2, id)
      .run();
  return sqlite3_changes(db_) > 0;
}

bool Db::delete_category(std::int64_t id) {
  bool deleted = false;
  transaction([&] {
    Stmt used(db_, "SELECT EXISTS(SELECT 1 FROM sessions WHERE category_id = ?)");
    used.bind(1, id).step();
    if (used.int_col(0) != 0) return;
    Stmt(db_, "DELETE FROM categories WHERE id = ?").bind(1, id).run();
    deleted = sqlite3_changes(db_) > 0;
  });
  return deleted;
}

// ---- daily goal history -----------------------------------------------------

std::vector<GoalChange> Db::goal_changes() const {
  Stmt s(db_, "SELECT from_day, minutes FROM goal_changes ORDER BY from_day");
  std::vector<GoalChange> out;
  while (s.step()) {
    const auto day = parse_date(s.text_col(0));
    if (!day) throw DbError("bad date in goal_changes: " + s.text_col(0));
    out.push_back({*day, static_cast<int>(s.int_col(1))});
  }
  return out;
}

void Db::set_goal_from(Date day, int minutes) {
  if (minutes <= 0) throw std::invalid_argument("goal minutes must be positive");
  Stmt(db_, "INSERT OR REPLACE INTO goal_changes(from_day, minutes) VALUES (?, ?)")
      .bind(1, format_date(day))
      .bind(2, std::int64_t{minutes})
      .run();
}

// ---- sessions ---------------------------------------------------------------

std::int64_t Db::insert_session(const Session& s) {
  if (s.end_utc < s.start_utc) throw std::invalid_argument("session end_utc < start_utc");
  if (s.duration_sec < 0) throw std::invalid_argument("session duration_sec < 0");
  Stmt(db_,
       "INSERT INTO sessions(start_utc, end_utc, duration_sec, category_id, note, completed) "
       "VALUES (?, ?, ?, ?, ?, ?)")
      .bind(1, s.start_utc)
      .bind(2, s.end_utc)
      .bind(3, s.duration_sec)
      .bind(4, s.category_id)
      .bind(5, s.note)
      .bind(6, std::int64_t{s.completed})
      .run();
  return sqlite3_last_insert_rowid(db_);
}

std::optional<Session> Db::session(std::int64_t id) const {
  Stmt s(db_, (std::string("SELECT ") + kSessionColumns + " FROM sessions WHERE id = ?").c_str());
  s.bind(1, id);
  if (!s.step()) return std::nullopt;
  return read_session(s);
}

std::vector<Session> Db::sessions_between(UnixSeconds start, UnixSeconds end) const {
  Stmt s(db_, (std::string("SELECT ") + kSessionColumns +
               " FROM sessions WHERE start_utc >= ? AND start_utc < ? ORDER BY start_utc, id")
                  .c_str());
  s.bind(1, start).bind(2, end);
  std::vector<Session> out;
  while (s.step()) out.push_back(read_session(s));
  return out;
}

std::vector<Session> Db::all_sessions() const {
  Stmt s(db_, (std::string("SELECT ") + kSessionColumns +
               " FROM sessions ORDER BY start_utc, id")
                  .c_str());
  std::vector<Session> out;
  while (s.step()) out.push_back(read_session(s));
  return out;
}

bool Db::update_session_category(std::int64_t id, std::int64_t category_id) {
  Stmt(db_, "UPDATE sessions SET category_id = ? WHERE id = ?").bind(1, category_id).bind(2, id).run();
  return sqlite3_changes(db_) > 0;
}

bool Db::update_session_note(std::int64_t id, std::string_view note) {
  Stmt(db_, "UPDATE sessions SET note = ? WHERE id = ?").bind(1, note).bind(2, id).run();
  return sqlite3_changes(db_) > 0;
}

bool Db::delete_session(std::int64_t id) {
  Stmt(db_, "DELETE FROM sessions WHERE id = ?").bind(1, id).run();
  return sqlite3_changes(db_) > 0;
}

}  // namespace mattrack
