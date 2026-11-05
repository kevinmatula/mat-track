# Third-party dependencies. Everything not available from the system is
# fetched with FetchContent, pinned to an exact tag / hash.
include(FetchContent)

# ---- SQLite: prefer system libsqlite3-dev, fall back to the amalgamation ------
find_package(SQLite3 QUIET)
if(SQLite3_FOUND)
  message(STATUS "Using system SQLite3 ${SQLite3_VERSION}")
else()
  message(STATUS "System SQLite3 not found; fetching amalgamation 3.54.1")
  FetchContent_Declare(sqlite_amalgamation
    URL https://www.sqlite.org/2026/sqlite-amalgamation-3540100.zip
    URL_HASH SHA3_256=92281c86e2957f9b9305890f9ac04204caa7b677cc868dc326d942689df28e30
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  )
  FetchContent_MakeAvailable(sqlite_amalgamation)

  find_package(Threads REQUIRED)
  add_library(sqlite3_vendored STATIC ${sqlite_amalgamation_SOURCE_DIR}/sqlite3.c)
  target_include_directories(sqlite3_vendored SYSTEM PUBLIC ${sqlite_amalgamation_SOURCE_DIR})
  target_compile_definitions(sqlite3_vendored PRIVATE
    SQLITE_DQS=0
    SQLITE_OMIT_LOAD_EXTENSION
  )
  target_link_libraries(sqlite3_vendored PUBLIC Threads::Threads)
  add_library(SQLite::SQLite3 ALIAS sqlite3_vendored)
endif()

# ---- SDL3 + Dear ImGui (app only) -------------------------------------------
if(MAT_TRACK_BUILD_APP)
  set(SDL_SHARED OFF CACHE BOOL "" FORCE)
  set(SDL_STATIC ON CACHE BOOL "" FORCE)
  set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
  set(SDL_TESTS OFF CACHE BOOL "" FORCE)
  set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG        release-3.4.18
    GIT_SHALLOW    TRUE
  )
  FetchContent_MakeAvailable(SDL3)

  FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.93.0
    GIT_SHALLOW    TRUE
  )
  FetchContent_MakeAvailable(imgui)

  # ImGui ships no CMake build; compile the core + SDL3/SDL_Renderer backends.
  add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdlrenderer3.cpp
  )
  target_include_directories(imgui SYSTEM PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
  )
  target_compile_definitions(imgui PUBLIC IMGUI_DISABLE_OBSOLETE_FUNCTIONS)
  target_link_libraries(imgui PUBLIC SDL3::SDL3)
endif()

# ---- Catch2 (tests only) ----------------------------------------------------
if(MAT_TRACK_BUILD_TESTS)
  FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.16.1
    GIT_SHALLOW    TRUE
  )
  FetchContent_MakeAvailable(Catch2)
  list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
endif()
