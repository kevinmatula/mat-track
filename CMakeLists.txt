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
  src/core/timer.cpp
  src/core/stats.cpp
  src/core/gamification.cpp
  src/core/file_util.cpp
  src/core/recovery.cpp
  src/core/heatmap.cpp
  src/core/demo.cpp
  src/core/goals.cpp
)
target_include_directories(mat_track_core PUBLIC src)
target_link_libraries(mat_track_core PUBLIC SQLite::SQLite3)
mat_track_warnings(mat_track_core)

# ---- app ----------------------------------------------------------------------
if(MAT_TRACK_BUILD_APP)
  # Assets are compiled into the binary so it runs from anywhere.
  function(mat_track_embed out_var name file)
    set(out ${CMAKE_CURRENT_BINARY_DIR}/embedded/${name}.cpp)
    add_custom_command(
      OUTPUT ${out}
      COMMAND ${CMAKE_COMMAND} -DIN=${file} -DOUT=${out} -DNAME=${name}
              -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedFile.cmake
      DEPENDS ${file} ${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedFile.cmake
      COMMENT "Embedding ${name}"
      VERBATIM)
    set(${out_var} ${out} PARENT_SCOPE)
  endfunction()
  mat_track_embed(font_src font_ttf ${CMAKE_CURRENT_SOURCE_DIR}/assets/font.ttf)
  mat_track_embed(chime_src chime_wav ${CMAKE_CURRENT_SOURCE_DIR}/assets/chime.wav)
  mat_track_embed(break_end_src break_end_wav ${CMAKE_CURRENT_SOURCE_DIR}/assets/break_end.wav)
  mat_track_embed(icon_src icon_bmp ${CMAKE_CURRENT_SOURCE_DIR}/assets/icon.bmp)

  add_executable(mat-track
    src/app/main.cpp
    src/app/app.cpp
    src/app/theme.cpp
    src/app/audio.cpp
    src/app/ui_timer.cpp
    src/app/ui_heatmap.cpp
    src/app/ui_day_panel.cpp
    src/app/ui_categories.cpp
    src/app/ui_stats.cpp
    src/app/ui_settings.cpp
    src/app/window_util.cpp
    ${font_src}
    ${chime_src}
    ${break_end_src}
    ${icon_src}
  )
  target_link_libraries(mat-track PRIVATE mat_track_core imgui SDL3::SDL3)
  # Used to bring the window to the front past focus-stealing prevention.
  find_package(X11)
  if(X11_FOUND)
    target_link_libraries(mat-track PRIVATE X11::X11)
    target_compile_definitions(mat-track PRIVATE MAT_TRACK_HAVE_X11)
  endif()
  mat_track_warnings(mat-track)

  # Install: the binary, a desktop menu entry and its icon (see install.sh).
  include(GNUInstallDirs)
  install(TARGETS mat-track RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
  set(MAT_TRACK_BIN ${CMAKE_INSTALL_FULL_BINDIR}/mat-track)
  configure_file(packaging/mat-track.desktop.in ${CMAKE_CURRENT_BINARY_DIR}/mat-track.desktop @ONLY)
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/mat-track.desktop
          DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
  install(FILES assets/icon.svg
          DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
          RENAME mat-track.svg)
endif()

# ---- tests ------------------------------------------------------------------
if(MAT_TRACK_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
