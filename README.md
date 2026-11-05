# mat-track

A minimal, local-only, black-and-white study tracker for Linux: pomodoro
timer, a GitHub-style heatmap of study history, and light gamification.

> Work in progress. The core (rollover-aware time handling, SQLite storage,
> timer state machine, stats, gamification, config) is done and tested. The
> app has a working pomodoro timer that logs sessions; the heatmap, day
> panel and gamification UI are still to come.

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
written with defaults on first run. While a session runs it also holds
`active_session.json`; if the app is killed, the next start offers to save
the interrupted session.

To try a full session quickly, put fractional minutes in `config.toml`:

```toml
focus_minutes = 0.5   # 30 s session
break_minutes = 0.25
```

Completed sessions are always logged; sessions stopped early are logged only
if they ran at least `min_log_seconds` (default 60).

## Sanitizers

```sh
cmake -S . -B build-asan -G Ninja -DCMAKE_BUILD_TYPE=Debug -DMAT_TRACK_SANITIZE=ON
cmake --build build-asan && ctest --test-dir build-asan
LSAN_OPTIONS=suppressions=tools/lsan.supp ./build-asan/mat-track --data-dir /tmp/mt
```

`tools/lsan.supp` silences one known leak inside SDL3's PulseAudio backend.

## Sounds

When a focus session completes, a jingle repeats (and the taskbar entry
flashes) until you click **Start break** or **Skip break**; when a break
ends, a second jingle repeats until **Start** or **Dismiss**. Meanwhile the
window is restored if minimized, focused, and kept above other windows. `sound_enabled = false` silences the jingles;
`raise_on_alarm = false` stops the window from popping up.

`assets/chime.wav` and `assets/break_end.wav` are generated sine jingles:
`python3 tools/gen_sounds.py` rewrites them.

## Licenses

The bundled font is IBM Plex Mono, under the SIL Open Font License 1.1
(`assets/font-LICENSE.txt`).
