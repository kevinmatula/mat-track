# mat-track

A minimal, local-only study tracker for Linux, in a grayscale Win9x style: a
pomodoro timer that logs your sessions, a heatmap of your study history, and
a little gamification (levels, streaks, grades, records). No accounts, no
network; everything stays in one SQLite file on your machine.

## Install

```sh
sudo apt install build-essential cmake ninja-build git \
  libasound2-dev libpulse-dev \
  libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxfixes-dev libxi-dev \
  libxss-dev libxtst-dev libxkbcommon-dev \
  libwayland-dev wayland-protocols libdecor-0-dev \
  libdrm-dev libgbm-dev libegl1-mesa-dev libgl1-mesa-dev libdbus-1-dev libudev-dev
./install.sh
```

`install.sh` builds a Release version and installs, for your user only:

- `~/.local/bin/mat-track`
- `~/.local/share/applications/mat-track.desktop` (so it shows up in the Mint menu)
- `~/.local/share/icons/hicolor/scalable/apps/mat-track.svg`

`./install.sh --uninstall` removes exactly those files. Neither touches your
data. `PREFIX=/some/dir ./install.sh` installs somewhere else.

## Using it

- **Timer**: pick a category and press **Start** (or Space). When the
  session ends, a jingle repeats and the window comes to the front until you
  click **Start break** or **Skip break**; when the break ends, another
  jingle plays until **Start** or **Dismiss**. After a session you can add a
  note right in the timer panel.
- **Header**: your level and XP (1 XP per minute studied), current streak,
  and today's grade. A banner appears when a session sets a new record.
- **History**: Year, Month and Week views of shaded cells (lighter = more of
  your daily goal). Hover a day for its session count; click it to list and
  edit its sessions. The Category dropdown filters the history.
- **Overview**: with no day selected, this week's total and grade, and your
  all-time records.
- **Settings...** (or F3): daily goal, streak minimum, timer lengths and
  alerts. A new daily goal applies from today on; past days keep the grades
  they earned.
- **Categories**: the **Edit...** button next to the timer's category. Archive
  a category to hide it from the timer while keeping its history.

### Keyboard

| Key | Action |
|---|---|
| Space | Start / pause / resume (or start the break / next session when it rings) |
| Esc | Stop the session (asks first if it's over a minute) |
| S | Skip the break |
| `[` / `]` | Previous / next category |
| F2 | Invert colors (dark variant) |
| F3 | Settings |
| Ctrl+Q | Quit (asks first if a session is running) |

Closing the window during a session asks: **Save and quit** (logs it as not
completed), **Keep running**, or **Discard**.

## Your data

Everything lives in `~/.local/share/mat-track/`:

- `mat-track.db`: every session (UTC times, so time zone changes are safe),
  categories, and the history of your daily goal. Back this file up to keep
  your history.
- `config.toml`: settings, written with defaults on first run. Most are in
  the Settings dialog; a few are file-only:

  | Key | Default | |
  |---|---|---|
  | `day_rollover_hour` | `4` | A study day starts at this local hour, so 1am counts as yesterday |
  | `week_starts_on` | `"monday"` | Or `"sunday"` |
  | `min_log_seconds` | `60` | Sessions stopped earlier than this aren't logged |
  | `max_session_minutes` | `240` | A session longer than this stops itself |
  | `auto_start_next` | `false` | Start the next session when a break ends |

- `active_session.json`: only while a session runs. If the app is killed,
  the next start offers to save the interrupted session.

## Development

```sh
cmake -S . -B build -G Ninja
cmake --build build && ctest --test-dir build
./build/mat-track --data-dir /tmp/mt                     # keep test data separate
./build/mat-track --data-dir /tmp/demo --seed-demo       # ~6 months of sample data
```

`--seed-demo` only works with `--data-dir` and an empty database, so sample
sessions never reach your real history. For quick sessions, put fractional
minutes in that data dir's `config.toml` (`focus_minutes = 0.1` is 6 s).

The code is split into `src/core/` (no UI: time and rollover handling, the
SQLite layer, the timer state machine, stats and gamification, all unit
tested) and `src/app/` (SDL3 + Dear ImGui). SDL3, Dear ImGui and Catch2 are
fetched by CMake at pinned versions (`cmake/Dependencies.cmake`); SQLite comes
from `libsqlite3-dev` if installed, otherwise from the fetched amalgamation.

| CMake option | Default | |
|---|---|---|
| `MAT_TRACK_BUILD_APP` | `ON` | Build the app (`OFF` for a fast core-only loop) |
| `MAT_TRACK_BUILD_TESTS` | `ON` | Build the unit tests |
| `MAT_TRACK_SANITIZE` | `OFF` | `-fsanitize=address,undefined` (use with `Debug`) |

Under the sanitizers, run the app with the one known SDL3 PulseAudio leak
suppressed:

```sh
cmake -S . -B build-asan -G Ninja -DCMAKE_BUILD_TYPE=Debug -DMAT_TRACK_SANITIZE=ON
cmake --build build-asan && ctest --test-dir build-asan
LSAN_OPTIONS=suppressions=tools/lsan.supp ./build-asan/mat-track --data-dir /tmp/mt
```

The jingles and the icon are generated: `python3 tools/gen_sounds.py` and
`python3 tools/gen_icon.py` rewrite `assets/`.

## Licenses

The bundled font is IBM Plex Mono, under the SIL Open Font License 1.1
(`assets/font-LICENSE.txt`).
