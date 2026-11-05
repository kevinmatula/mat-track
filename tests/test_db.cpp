#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <random>

#include "core/db.hpp"

using namespace mattrack;
namespace fs = std::filesystem;

namespace {

// Unique scratch directory under the system temp dir; never the real data dir.
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

Session make_session(std::int64_t category_id, UnixSeconds start, std::int64_t duration) {
  Session s;
  s.start_utc = start;
  s.end_utc = start + duration;
  s.duration_sec = duration;
  s.category_id = category_id;
  return s;
}

std::int64_t general_id(const Db& db) { return db.categories().at(0).id; }

}  // namespace

TEST_CASE("fresh database has schema v1 and the default category", "[db]") {
  Db db(":memory:");
  CHECK(db.schema_version() == Db::kSchemaVersion);
  const auto cats = db.categories();
  REQUIRE(cats.size() == 1);
  CHECK(cats[0].name == Db::kDefaultCategory);
  CHECK_FALSE(cats[0].archived);
  CHECK(db.all_sessions().empty());
}

TEST_CASE("reopening a database is idempotent and keeps data", "[db]") {
  TempDir dir;
  const std::string path = (dir.path / "mat-track.db").string();
  std::int64_t session_id = 0;
  {
    Db db(path);
    db.add_category("Math");
    session_id = db.insert_session(make_session(general_id(db), 1000, 1500));
  }
  {
    Db db(path);
    CHECK(db.schema_version() == Db::kSchemaVersion);
    CHECK(db.categories().size() == 2);  // General not seeded twice
    CHECK(db.session(session_id).has_value());
  }
}

TEST_CASE("database from a newer schema version is rejected", "[db]") {
  TempDir dir;
  const std::string path = (dir.path / "mat-track.db").string();
  {
    Db db(path);
    db.exec("PRAGMA user_version = 99");
  }
  CHECK_THROWS_AS(Db(path), DbError);
}

TEST_CASE("open failure throws DbError", "[db]") {
  TempDir dir;
  CHECK_THROWS_AS(Db((dir.path / "missing-dir" / "x.db").string()), DbError);
}

TEST_CASE("session insert/query roundtrip", "[db]") {
  Db db(":memory:");
  const auto cat = general_id(db);

  Session s{.start_utc = 1718509800,
            .end_utc = 1718513400,
            .duration_sec = 3000,  // less than end-start: paused for 10 min
            .category_id = cat,
            .note = "chapter 3 — ünïcödé",
            .completed = true};
  const auto id = db.insert_session(s);
  CHECK(id > 0);

  s.id = id;
  CHECK(db.session(id) == s);
  CHECK(db.all_sessions() == std::vector<Session>{s});
  CHECK_FALSE(db.session(id + 1).has_value());
}

TEST_CASE("sessions_between is half-open on start_utc and ordered", "[db]") {
  Db db(":memory:");
  const auto cat = general_id(db);
  const auto late = db.insert_session(make_session(cat, 300, 60));
  const auto early = db.insert_session(make_session(cat, 100, 60));
  db.insert_session(make_session(cat, 400, 60));  // at the end bound: excluded
  db.insert_session(make_session(cat, 99, 60));   // before the start: excluded

  const auto got = db.sessions_between(100, 400);
  REQUIRE(got.size() == 2);
  CHECK(got[0].id == early);
  CHECK(got[1].id == late);
}

TEST_CASE("invalid sessions are rejected", "[db]") {
  Db db(":memory:");
  const auto cat = general_id(db);
  Session backwards = make_session(cat, 1000, 60);
  backwards.end_utc = 999;
  CHECK_THROWS_AS(db.insert_session(backwards), std::invalid_argument);

  Session negative = make_session(cat, 1000, 60);
  negative.duration_sec = -1;
  CHECK_THROWS_AS(db.insert_session(negative), std::invalid_argument);
}

TEST_CASE("foreign keys are enforced", "[db]") {
  Db db(":memory:");
  CHECK_THROWS_AS(db.insert_session(make_session(12345, 1000, 60)), DbError);
  const auto id = db.insert_session(make_session(general_id(db), 1000, 60));
  CHECK_THROWS_AS(db.update_session_category(id, 12345), DbError);
  CHECK(db.session(id)->category_id == general_id(db));
}

TEST_CASE("session edits and delete", "[db]") {
  Db db(":memory:");
  const auto math = db.add_category("Math");
  const auto id = db.insert_session(make_session(general_id(db), 1000, 600));

  CHECK(db.update_session_category(id, math));
  CHECK(db.update_session_note(id, "proofs"));
  const auto s = db.session(id);
  CHECK(s->category_id == math);
  CHECK(s->note == "proofs");

  CHECK(db.delete_session(id));
  CHECK_FALSE(db.session(id).has_value());
  CHECK_FALSE(db.delete_session(id));
  CHECK_FALSE(db.update_session_note(id, "x"));
}

TEST_CASE("category names: trimmed, non-empty, unique ignoring case", "[db]") {
  Db db(":memory:");
  const auto id = db.add_category("  Physics \t");
  CHECK(db.category(id)->name == "Physics");

  CHECK_THROWS_AS(db.add_category("physics"), DbError);
  CHECK_THROWS_AS(db.add_category("   "), std::invalid_argument);
  CHECK_THROWS_AS(db.rename_category(id, "GENERAL"), DbError);
  CHECK_THROWS_AS(db.rename_category(id, ""), std::invalid_argument);

  CHECK(db.rename_category(id, "Mechanics"));
  CHECK(db.category(id)->name == "Mechanics");
  CHECK_FALSE(db.rename_category(9999, "Nope"));
}

TEST_CASE("categories are listed by name; archived ones can be hidden", "[db]") {
  Db db(":memory:");
  const auto zoo = db.add_category("zoology");
  db.add_category("Art");

  auto names = [](const std::vector<Category>& cs) {
    std::vector<std::string> out;
    for (const auto& c : cs) out.push_back(c.name);
    return out;
  };
  CHECK(names(db.categories()) == std::vector<std::string>{"Art", "General", "zoology"});

  CHECK(db.set_category_archived(zoo, true));
  CHECK(db.category(zoo)->archived);
  CHECK(names(db.categories(false)) == std::vector<std::string>{"Art", "General"});
  CHECK(names(db.categories(true)).size() == 3);

  CHECK(db.set_category_archived(zoo, false));
  CHECK(names(db.categories(false)).size() == 3);
}

TEST_CASE("archive vs delete", "[db]") {
  Db db(":memory:");
  const auto used = db.add_category("Used");
  const auto unused = db.add_category("Unused");
  const auto sid = db.insert_session(make_session(used, 1000, 600));

  SECTION("a category with sessions cannot be deleted, only archived") {
    CHECK_FALSE(db.delete_category(used));
    CHECK(db.category(used).has_value());
    CHECK(db.set_category_archived(used, true));
    // History still references it.
    CHECK(db.session(sid)->category_id == used);
    CHECK(db.category(used)->archived);
  }
  SECTION("a category without sessions can be deleted") {
    CHECK(db.delete_category(unused));
    CHECK_FALSE(db.category(unused).has_value());
    CHECK_FALSE(db.delete_category(unused));
  }
  SECTION("raw DELETE of a referenced category is blocked by the foreign key") {
    CHECK_THROWS_AS(db.exec(("DELETE FROM categories WHERE id = " + std::to_string(used)).c_str()),
                    DbError);
  }
}

TEST_CASE("transactions roll back on exception and can nest", "[db]") {
  Db db(":memory:");
  const auto cat = general_id(db);

  CHECK_THROWS_AS(db.transaction([&] {
    db.insert_session(make_session(cat, 1000, 60));
    throw std::runtime_error("boom");
  }),
                  std::runtime_error);
  CHECK(db.all_sessions().empty());

  db.transaction([&] {
    db.insert_session(make_session(cat, 1000, 60));
    db.transaction([&] { db.insert_session(make_session(cat, 2000, 60)); });
    CHECK_FALSE(db.delete_category(cat));  // nests inside the outer transaction
  });
  CHECK(db.all_sessions().size() == 2);
}
