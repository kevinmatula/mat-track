# mat-track

A minimal, local-only, black-and-white study tracker for Linux: pomodoro
timer, a GitHub-style heatmap of study history, and light gamification.

> Work in progress. The core (rollover-aware time handling, SQLite storage,
> timer state machine, stats, gamification, config) is done and tested; the
> app currently opens an empty black-and-white shell window.

## Build dependencies (Linux Mint 22 / Ubuntu 24.04)

```sh
sudo apt install build-essential cmake ninja-build git \
  libsqlite3-dev \
  libasound2-dev libpulse-dev \
  libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxfixes-dev libxi-dev \
  libxss-dev libxtst-dev libxkbcommon-dev \
  libwayland-dev wayland-protocols libdecor-0-dev \
  libdrm-dev libgbm-dev libegl1-mesa-dev libgl1-mesa-dev libdbus-1-dev libudev-dev
```

`libsqlite3-dev` is optional: without it, the SQLite amalgamation is
downloaded and compiled in. SDL3, Dear ImGui and Catch2 are always fetched
by CMake (pinned versions in `cmake/Dependencies.cmake`).

## Build and test

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build
```

CMake options:

| Option | Default | |
|---|---|---|
| `MAT_TRACK_BUILD_APP` | `ON` | Fetch/build SDL3 + ImGui and the app |
| `MAT_TRACK_BUILD_TESTS` | `ON` | Fetch Catch2 and build the unit tests |
| `MAT_TRACK_SANITIZE` | `OFF` | Build with `-fsanitize=address,undefined` (use with `Debug`) |

For a fast core-only loop: `-DMAT_TRACK_BUILD_APP=OFF`.

## Run

```sh
./build/mat-track                        # data in ~/.local/share/mat-track/
./build/mat-track --data-dir /tmp/mt     # separate data dir for testing
```

The data directory holds `mat-track.db` (SQLite) and `config.toml`, which is
written with defaults on first run.

## Licenses

The bundled font is IBM Plex Mono, under the SIL Open Font License 1.1
(`assets/font-LICENSE.txt`).
