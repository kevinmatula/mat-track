#include "app/app.hpp"

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

#include <cmath>
#include <cstdio>
#include <system_error>

#include "app/theme.hpp"

namespace mattrack {

namespace {

constexpr const char* kAppName = "mat-track";
constexpr int kDefaultWidth = 1100;
constexpr int kDefaultHeight = 700;
constexpr int kMinWidth = 900;
constexpr int kMinHeight = 600;

// Main loop pacing: render at full (vsync) rate for a while after any input,
// otherwise sleep on the event queue and redraw about once per second.
constexpr auto kActiveAfterInput = std::chrono::milliseconds(1000);
constexpr int kIdleWaitMs = 250;
constexpr auto kIdleRedraw = std::chrono::milliseconds(1000);

}  // namespace

App::App(AppOptions options) : options_(std::move(options)) {}

App::~App() {
  if (imgui_ready_) {
    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
  }
  if (renderer_) SDL_DestroyRenderer(renderer_);
  if (window_) SDL_DestroyWindow(window_);
  if (sdl_ready_) SDL_Quit();
}

void App::fail(const std::string& message) {
  std::fprintf(stderr, "mat-track: %s\n", message.c_str());
  SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, kAppName, message.c_str(), window_);
}

bool App::init_data() {
  if (options_.data_dir) {
    data_dir_ = *options_.data_dir;
  } else {
    // An empty org gives ~/.local/share/mat-track/ (not .../mat-track/mat-track/).
    char* pref = SDL_GetPrefPath("", kAppName);
    if (!pref) {
      fail(std::string("Cannot determine the data directory: ") + SDL_GetError());
      return false;
    }
    data_dir_ = pref;
    SDL_free(pref);
  }

  std::error_code ec;
  std::filesystem::create_directories(data_dir_, ec);
  if (ec) {
    fail("Cannot create data directory " + data_dir_.string() + ": " + ec.message());
    return false;
  }

  try {
    std::vector<std::string> warnings;
    config_ = load_or_create_config(data_dir_ / "config.toml", &warnings);
    for (const auto& w : warnings) std::fprintf(stderr, "mat-track: %s\n", w.c_str());
  } catch (const std::exception& e) {
    fail(std::string("Cannot load settings.\n\n") + e.what());
    return false;
  }

  try {
    db_.emplace((data_dir_ / "mat-track.db").string());
  } catch (const std::exception& e) {
    fail(std::string("Cannot open the database. No data was changed.\n\n") + e.what());
    return false;
  }
  return true;
}

bool App::init_window() {
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    fail(std::string("SDL_Init failed: ") + SDL_GetError());
    return false;
  }
  sdl_ready_ = true;

  const float content_scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
  const float s = content_scale > 0 ? content_scale : 1.0f;
  const SDL_WindowFlags flags =
      SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY;
  window_ = SDL_CreateWindow(kAppName, static_cast<int>(kDefaultWidth * s),
                             static_cast<int>(kDefaultHeight * s), flags);
  if (!window_) {
    fail(std::string("Cannot create window: ") + SDL_GetError());
    return false;
  }
  SDL_SetWindowMinimumSize(window_, static_cast<int>(kMinWidth * s),
                           static_cast<int>(kMinHeight * s));

  renderer_ = SDL_CreateRenderer(window_, nullptr);
  if (!renderer_) {
    fail(std::string("Cannot create renderer: ") + SDL_GetError());
    return false;
  }
  vsync_ = SDL_SetRenderVSync(renderer_, 1);
  SDL_SetWindowPosition(window_, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.IniFilename = nullptr;  // single fixed layout; nothing to persist
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  ImGui_ImplSDL3_InitForSDLRenderer(window_, renderer_);
  ImGui_ImplSDLRenderer3_Init(renderer_);
  imgui_ready_ = true;

  ui::load_fonts();
  update_ui_scale();
  ui::apply_theme(config_.invert_colors, ui_scale_);

  SDL_ShowWindow(window_);
  return true;
}

bool App::init() { return init_data() && init_window(); }

void App::update_ui_scale() {
  // ImGui works in window coordinates; the backend handles pixel density
  // (e.g. Wayland 2x). What's left to scale is the content scale.
  const float display = SDL_GetWindowDisplayScale(window_);
  const float density = SDL_GetWindowPixelDensity(window_);
  float scale = (display > 0 && density > 0) ? display / density : 1.0f;
  scale = std::round(scale * 4.0f) / 4.0f;  // avoid blurry fractional sizes
  if (scale <= 0) scale = 1.0f;
  if (scale != ui_scale_) {
    ui_scale_ = scale;
    ui::apply_theme(config_.invert_colors, ui_scale_);
  }
}

void App::process_event(const SDL_Event& event) {
  ImGui_ImplSDL3_ProcessEvent(&event);
  switch (event.type) {
    case SDL_EVENT_QUIT:
      running_ = false;
      break;
    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
      if (event.window.windowID == SDL_GetWindowID(window_)) running_ = false;
      break;
    case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
      update_ui_scale();
      break;
    default:
      break;
  }
}

int App::run() {
  if (!init()) return 1;

  last_activity_ = Clock::now();
  while (running_) {
    const bool active = Clock::now() - last_activity_ < kActiveAfterInput ||
                        ImGui::GetIO().WantTextInput;

    // Block on the event queue only when idle, so the app costs ~nothing in
    // the background but still wakes regularly (the timer must keep going
    // when unfocused or minimized).
    SDL_Event event;
    bool have_event = active ? SDL_PollEvent(&event) : SDL_WaitEventTimeout(&event, kIdleWaitMs);
    while (have_event) {
      process_event(event);
      last_activity_ = Clock::now();
      have_event = SDL_PollEvent(&event);
    }
    if (!running_) break;

    // Per-iteration work that must not depend on rendering (timer tick,
    // recovery file) goes here in later milestones.

    if (SDL_GetWindowFlags(window_) & SDL_WINDOW_MINIMIZED) continue;

    const auto now = Clock::now();
    if (now - last_activity_ < kActiveAfterInput || now - last_draw_ >= kIdleRedraw) {
      draw_frame();
      render();
      last_draw_ = now;
      if (!vsync_) SDL_Delay(16);
    }
  }
  return 0;
}

void App::draw_frame() {
  ImGui_ImplSDLRenderer3_NewFrame();
  ImGui_ImplSDL3_NewFrame();
  ImGui::NewFrame();

  const ImGuiViewport* vp = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(vp->WorkPos);
  ImGui::SetNextWindowSize(vp->WorkSize);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::Begin("##main", nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus);
  ImGui::PopStyleVar();

  // Placeholder content for the app shell; replaced by the real panels in
  // later milestones.
  ImGui::TextUnformatted("mat-track");
  ImGui::SameLine();
  ImGui::TextDisabled("app shell");
  ImGui::Separator();

  const int total = static_cast<int>(std::lround(config_.focus_minutes * 60));
  char digits[16];
  std::snprintf(digits, sizeof digits, "%02d:%02d", total / 60, total % 60);
  ui::push_timer_font();
  ImGui::TextUnformatted(digits);
  ui::pop_font();

  ui::button("Start");
  ImGui::SameLine();
  ui::button("Pause", {}, false);
  ImGui::SameLine();
  ui::button("Stop", {}, false);

  ImGui::Spacing();
  ImGui::TextDisabled("data: %s", data_dir_.string().c_str());
  ImGui::TextDisabled("scale %.2f  %s", ui_scale_, ui::inverted() ? "inverted" : "normal");

  ImGui::End();
  ImGui::Render();
}

void App::render() {
  const ImGuiIO& io = ImGui::GetIO();
  SDL_SetRenderScale(renderer_, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
  const float v = ui::inverted() ? 1.0f : 0.0f;
  SDL_SetRenderDrawColorFloat(renderer_, v, v, v, 1.0f);
  SDL_RenderClear(renderer_);
  ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer_);
  SDL_RenderPresent(renderer_);
}

}  // namespace mattrack
