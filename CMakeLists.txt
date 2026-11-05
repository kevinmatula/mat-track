cmake_minimum_required(VERSION 3.24)

project(mat-track VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

option(MAT_TRACK_BUILD_APP "Build the SDL3/ImGui desktop app (fetches SDL3 and Dear ImGui)" ON)
option(MAT_TRACK_BUILD_TESTS "Build unit tests (fetches Catch2)" ON)
option(MAT_TRACK_SANITIZE "Build with AddressSanitizer + UndefinedBehaviorSanitizer" OFF)

if(MAT_TRACK_SANITIZE)
  # Applied globally so dependencies are instrumented too.
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined)
endif()

include(cmake/Dependencies.cmake)

# Warnings for our own targets only (not third-party code).
function(mat_track_warnings target)
  target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
endfunction()

# ---- core library: no SDL / ImGui -------------------------------------------
add_library(mat_track_core STATIC
  src/core/time_util.cpp
  src/core/db.cpp
  src/core/config.cpp
)
target_include_directories(mat_track_core PUBLIC src)
target_link_libraries(mat_track_core PUBLIC SQLite::SQLite3)
mat_track_warnings(mat_track_core)

# ---- tests ------------------------------------------------------------------
if(MAT_TRACK_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
