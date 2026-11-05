#pragma once

#include <SDL3/SDL.h>

#include <chrono>
#include <filesystem>
#include <optional>
#include <string>

#include "core/config.hpp"
#include "core/db.hpp"

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

  bool init();
  bool init_data();
  bool init_window();
  void process_event(const SDL_Event& event);
  void update_ui_scale();
  void draw_frame();
  void render();
  void fail(const std::string& message);

  AppOptions options_;
  std::filesystem::path data_dir_;
  Config config_;
  std::optional<Db> db_;

  SDL_Window* window_ = nullptr;
  SDL_Renderer* renderer_ = nullptr;
  bool sdl_ready_ = false;
  bool imgui_ready_ = false;
  bool vsync_ = false;

  bool running_ = true;
  float ui_scale_ = 1.0f;
  Clock::time_point last_activity_{};
  Clock::time_point last_draw_{};
};

}  // namespace mattrack
