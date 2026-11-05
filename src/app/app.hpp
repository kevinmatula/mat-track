#pragma once

#include <SDL3/SDL.h>

#include <array>
#include <chrono>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "app/audio.hpp"
#include "core/config.hpp"
#include "core/db.hpp"
#include "core/recovery.hpp"
#include "core/timer.hpp"

namespace mattrack {

struct AppOptions {
  // Overrides the default ~/.local/share/mat-track/.
  std::optional<std::filesystem::path> data_dir;
};

// Owns SDL, ImGui, the config and the database, and runs the main loop.
class App {
 public:
  explicit App(AppOptions options);
  ~App();
  App(const App&) = delete;
  App& operator=(const App&) = delete;

  // Returns the process exit code.
  int run();

 private:
  using Clock = std::chrono::steady_clock;

  // The real clocks, read once per use and passed into the core.
  struct Now {
    MonoNs mono;
    UnixSeconds utc;
  };
  static Now now();

  bool init();
  bool init_data();
  bool init_window();
  void process_event(const SDL_Event& event);
  void update_ui_scale();
  void draw_frame();
  void render();
  void fail(const std::string& message);
  // True when the countdown's visible seconds changed since the last draw.
  bool countdown_changed() const;
  std::int64_t countdown_seconds() const;

  // ---- timer plumbing (app.cpp) ----
  void update_timer();
  void start_session();
  void pause_session();
  void resume_session();
  void stop_session();
  void start_break();
  void skip_break();
  // Repeating jingle + taskbar flash until the user responds.
  enum class Alarm { None, FocusDone, BreakDone };
  void ring(Alarm alarm);
  void silence();
  // Logs (or discards) the finished session, then moves the timer on.
  void handle_finished();
  void write_recovery(UnixSeconds utc);
  void reload_categories();

  // ---- crash recovery (app.cpp) ----
  std::filesystem::path recovery_path() const;
  void check_recovery_file();
  void save_recovered_session();
  void discard_recovered_session();

  // ---- panels (ui_timer.cpp) ----
  void draw_timer_panel();
  void draw_note_box();
  void draw_recovery_dialog();

  AppOptions options_;
  std::filesystem::path data_dir_;
  Config config_;
  std::optional<Db> db_;
  Audio audio_;

  SDL_Window* window_ = nullptr;
  SDL_Renderer* renderer_ = nullptr;
  bool sdl_ready_ = false;
  bool imgui_ready_ = false;
  bool vsync_ = false;

  bool running_ = true;
  float ui_scale_ = 1.0f;
  Clock::time_point last_activity_{};
  Clock::time_point last_draw_{};
  std::int64_t drawn_countdown_ = -1;

  Timer timer_{TimerSettings{}};
  std::vector<Category> categories_;  // non-archived, for the dropdown
  std::int64_t selected_category_ = 0;
  std::string status_ = "Ready";
  Alarm alarm_ = Alarm::None;
  bool raised_for_alarm_ = false;  // window held on top until the alarm ends

  // Inline note prompt for the session just logged.
  std::optional<std::int64_t> note_session_id_;
  std::array<char, 256> note_buf_{};

  // Recovery file bookkeeping for the active session.
  UnixSeconds recovery_for_start_ = -1;  // start_utc of the session on disk
  MonoNs last_recovery_write_ = 0;
  // An interrupted session found at startup, awaiting Save / Discard.
  std::optional<RecoveryState> pending_recovery_;
};

}  // namespace mattrack
