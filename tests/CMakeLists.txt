include(Catch)

add_executable(mat_track_tests
  test_time_util.cpp
  test_db.cpp
  test_config.cpp
  test_timer.cpp
  test_stats.cpp
  test_gamification.cpp
)
target_link_libraries(mat_track_tests PRIVATE mat_track_core Catch2::Catch2WithMain)
mat_track_warnings(mat_track_tests)

catch_discover_tests(mat_track_tests)
