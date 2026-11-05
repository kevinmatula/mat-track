#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <random>

#include "core/file_util.hpp"
#include "core/recovery.hpp"

using namespace mattrack;
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

const RecoveryState kState{.start_utc = 1718438400,
                           .category_id = 3,
                           .focused_seconds = 1234,
                           .updated_utc = 1718440000};

}  // namespace

TEST_CASE("recovery state roundtrips through JSON", "[recovery]") {
  const std::string json = serialize_recovery(kState);
  CHECK(parse_recovery(json) == kState);
}

TEST_CASE("recovery parsing tolerates formatting, rejects garbage", "[recovery]") {
  CHECK(parse_recovery(R"({"updated_utc":1718440000,"focused_seconds" : 1234,)"
                       R"( "category_id":3, "start_utc":1718438400})") == kState);

  CHECK_FALSE(parse_recovery(""));
  CHECK_FALSE(parse_recovery("not json"));
  CHECK_FALSE(parse_recovery(R"({"start_utc": 1, "category_id": 2, "focused_seconds": 3})"));
  CHECK_FALSE(parse_recovery(
      R"({"start_utc": 1, "category_id": 2, "focused_seconds": 3.5, "updated_utc": 4})"));
  CHECK_FALSE(parse_recovery(
      R"({"start_utc": 1, "category_id": 2, "focused_seconds": -3, "updated_utc": 4})"));
  // A file cut off mid-write.
  const std::string json = serialize_recovery(kState);
  CHECK_FALSE(parse_recovery(json.substr(0, json.size() / 2)));
}

TEST_CASE("recovered session is not completed and never ends before it starts", "[recovery]") {
  const Session s = recovered_session(kState);
  CHECK(s.start_utc == kState.start_utc);
  CHECK(s.end_utc == kState.updated_utc);
  CHECK(s.duration_sec == 1234);
  CHECK(s.category_id == 3);
  CHECK_FALSE(s.completed);

  RecoveryState backwards = kState;
  backwards.updated_utc = kState.start_utc - 100;  // clock jumped back
  CHECK(recovered_session(backwards).end_utc == kState.start_utc);
}

TEST_CASE("recovery file save / load / remove", "[recovery]") {
  TempDir dir;
  const fs::path path = dir.path / "active_session.json";

  CHECK_FALSE(read_file(path));
  save_recovery(kState, path);
  REQUIRE(read_file(path));
  CHECK(parse_recovery(*read_file(path)) == kState);
  CHECK_FALSE(fs::exists(dir.path / "active_session.json.tmp"));

  RecoveryState later = kState;
  later.focused_seconds = 2000;
  save_recovery(later, path);
  CHECK(parse_recovery(*read_file(path)) == later);

  remove_recovery(path);
  CHECK_FALSE(fs::exists(path));
  remove_recovery(path);  // removing a missing file is fine
}
