#include "app/app.hpp"

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <system_error>

#include "app/theme.hpp"
#include "app/window_util.hpp"
#include "core/demo.hpp"
#include "core/file_util.hpp"
#include "core/time_util.hpp"

namespace mattrack {

namespace {

constexpr const char* kAppName = "mat-track";

// Main loop pacing: render at full (vsync) rate for a while after any input,
// otherwise sleep on the event queue and redraw about once per second.
constexpr auto kActiveAfterInput = std::chrono::milliseconds(1000);
constexpr int kIdleWaitMs = 250;
constexpr auto kIdleRedraw = std::chrono::milliseconds(1000);

// How often the crash-recovery file is refreshed while a session runs.
constexpr MonoNs kRecoveryInterval = 15 * kNsPerSecond;

}  // namespace

App::App(AppOptions options) : options_(std::move(options)) {}

App::~App() {
  audio_.shutdown();  // SDL resources must go before SDL_Quit()
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

  try {
    tz_ = std::chrono::current_zone();
  } catch (const std::exception& e) {
    fail(std::string("Cannot determine the local time zone.\n\n") + e.what());
    return false;
  }

  if (options_.seed_demo) {
    try {
      if (!db_->all_sessions().empty()) {
        fail("--seed-demo needs an empty database; " + data_dir_.string() +
             " already has sessions.");
        return false;
      }
      const int n = seed_demo(*db_, now().utc, *tz_, config_.day_rollover_hour);
      std::fprintf(stderr, "mat-track: seeded %d demo sessions into %s\n", n,
                   data_dir_.string().c_str());
    } catch (const std::exception& e) {
      fail(std::string("Cannot seed demo data.\n\n") + e.what());
      return false;
    }
  }

  timer_ = Timer(TimerSettings::from_config(config_));
  try {
    // config.toml may have been edited since the last run.
    sync_goal(*db_, today(), config_.daily_goal_minutes);
    reload_categories();
    reload_history();
  } catch (const std::exception& e) {
    fail(std::string("Cannot read the session history.\n\n") + e.what());
    return false;
  }
  check_recovery_file();
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
  window_ = SDL_CreateWindow(kAppName, static_cast<int>(kDefaultWindowWidth * s),
                             static_cast<int>(kDefaultWindowHeight * s), flags);
  if (!window_) {
    fail(std::string("Cannot create window: ") + SDL_GetError());
    return false;
  }
  SDL_SetWindowMinimumSize(window_, static_cast<int>(kMinWindowWidth * s),
                           static_cast<int>(kMinWindowHeight * s));

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

  audio_.init();  // optional; failures are logged and sounds stay silent

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

    // Runs every iteration, drawn or not, so the timer keeps going while
    // the window is unfocused or minimized.
    update_timer();
    audio_.update();

    if (SDL_GetWindowFlags(window_) & SDL_WINDOW_MINIMIZED) continue;

    const auto now = Clock::now();
    if (now - last_activity_ < kActiveAfterInput || now - last_draw_ >= kIdleRedraw ||
        countdown_changed()) {
      draw_frame();
      render();
      last_draw_ = now;
      if (!vsync_) SDL_Delay(16);
    }
  }

  // Quitting mid-session keeps the recovery file, so the next start offers
  // to save the session. (A proper "save and quit" prompt comes later.)
  if (timer_.state() == TimerState::Running || timer_.state() == TimerState::Paused) {
    const Now n = now();
    timer_.tick(n.mono, n.utc);
    write_recovery(n.utc);
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

  // Everything above the status bar scrolls if the window is too short for
  // it, so it never runs under the status bar.
  ImGui::BeginChild("##content", {0, -ui::status_bar_height()}, ImGuiChildFlags_None);

  // Like the history panel, rows stop growing at their default-window width.
  const float row_w = kDefaultWindowWidth * ui_scale_ - 2 * ImGui::GetStyle().WindowPadding.x;
  draw_header(std::min(ui::avail_width(), row_w));
  ImGui::Spacing();

  // The timer, and the selected day (or the overview) beside it.
  draw_timer_panel();
  timer_panel_h_ = ImGui::GetItemRectSize().y;
  const float spacing = ImGui::GetStyle().ItemSpacing.x;
  ImGui::SameLine(0, spacing);
  const float day_w = std::min(ui::avail_width(), row_w - ImGui::GetItemRectSize().x - spacing);
  draw_day_panel(day_w, timer_panel_h_);

  draw_heatmap_panel();
  ImGui::EndChild();

  draw_recovery_dialog();
  draw_session_dialog();
  draw_categories_dialog();
  draw_settings_dialog();
  if (ImGui::IsKeyPressed(ImGuiKey_F3) && !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId)) {
    open_settings_dialog_ = true;
  }

  const std::string data = "data: " + data_dir_.string();
  ui::status_bar({status_.c_str(), data.c_str()});
  drawn_countdown_ = countdown_seconds();

  ImGui::End();
  ImGui::Render();
}

// ---- timer plumbing ---------------------------------------------------------

App::Now App::now() {
  using namespace std::chrono;
  return {duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count(),
          duration_cast<seconds>(system_clock::now().time_since_epoch()).count()};
}

std::int64_t App::countdown_seconds() const {
  const TimerState st = timer_.state();
  if (st != TimerState::Running && st != TimerState::Paused && st != TimerState::Break &&
      st != TimerState::BreakReady) {
    return -1;
  }
  // Round up: show 50:00 at the start and 00:00 only when time is up.
  return (timer_.remaining() + kNsPerSecond - 1) / kNsPerSecond;
}

bool App::countdown_changed() const { return countdown_seconds() != drawn_countdown_; }

void App::reload_categories() {
  categories_ = db_->categories(/*include_archived=*/false);
  const bool still_there =
      std::any_of(categories_.begin(), categories_.end(),
                  [&](const Category& c) { return c.id == selected_category_; });
  if (!still_there) selected_category_ = categories_.empty() ? 0 : categories_.front().id;
}

void App::reload_history() {
  sessions_ = db_->all_sessions();
  all_categories_ = db_->categories(/*include_archived=*/true);
  category_names_.clear();
  for (const auto& c : all_categories_) category_names_[c.id] = c.name;
  all_days_ = day_totals(sessions_, *tz_, config_.day_rollover_hour);
  goals_ = GoalHistory(db_->goal_changes(), config_.daily_goal_minutes);
  records_ = compute_records(sessions_, {.tz = tz_,
                                         .rollover_hour = config_.day_rollover_hour,
                                         .week_starts_on = config_.week_starts_on,
                                         .streak_min_minutes = config_.streak_min_minutes});
  apply_filter();
}

void App::note_new_records(const Records& before) {
  std::string text;
  for (const RecordKind kind : new_records(before, records_)) {
    std::string part;
    switch (kind) {
      case RecordKind::BestDay:
        part = "best day " + format_duration(records_.best_day->seconds);
        break;
      case RecordKind::LongestSession:
        part = "longest session " + format_duration(records_.longest_session->seconds);
        break;
      case RecordKind::BestWeek:
        continue;  // not shown in the overview, so no banner either
      case RecordKind::LongestStreak:
        part = "longest streak " + std::to_string(records_.longest_streak) + " days";
        break;
    }
    text += (text.empty() ? "" : ", ") + part;
  }
  if (!text.empty()) banner_ = "NEW RECORD: " + text;
}

void App::apply_filter() {
  // A filtered category that was deleted falls back to "All".
  if (filter_ && !category_names_.contains(*filter_)) filter_.reset();
  day_totals_ = day_totals(sessions_, *tz_, config_.day_rollover_hour, filter_);
}

std::vector<Session> App::sessions_on(Date day) const {
  std::vector<Session> out;
  for (const auto& s : sessions_) {
    if (filter_ && s.category_id != *filter_) continue;
    if (day_key(s.start_utc, *tz_, config_.day_rollover_hour) == day) out.push_back(s);
  }
  return out;
}

const char* App::category_name(std::int64_t id) const {
  const auto it = category_names_.find(id);
  return it == category_names_.end() ? "?" : it->second.c_str();
}

Date App::today() const { return day_key(now().utc, *tz_, config_.day_rollover_hour); }

void App::update_timer() {
  const Now n = now();
  // Settings changed mid-session take effect once the timer is idle.
  if (pending_timer_settings_ && timer_.set_settings(*pending_timer_settings_)) {
    pending_timer_settings_.reset();
  }
  const TimerState before = timer_.state();
  timer_.tick(n.mono, n.utc);

  if (timer_.state() == TimerState::Finished) handle_finished();
  if (before == TimerState::Break && timer_.state() != TimerState::Break) {
    if (timer_.state() == TimerState::Running) {  // auto_start_next
      status_ = "Break over: next session started.";
      if (config_.sound_enabled) audio_.play(Sound::BreakEnd);
    } else {
      status_ = "Break over.";
      ring(Alarm::BreakDone);
    }
  }

  const TimerState st = timer_.state();
  if (st == TimerState::Running || st == TimerState::Paused) {
    // A new session (including an auto-started one) or a periodic refresh.
    if (recovery_for_start_ != timer_.start_utc() ||
        n.mono - last_recovery_write_ >= kRecoveryInterval) {
      write_recovery(n.utc);
    }
  }
}

void App::write_recovery(UnixSeconds utc) {
  const RecoveryState state{.start_utc = timer_.start_utc(),
                            .category_id = timer_.category_id(),
                            .focused_seconds = timer_.focused() / kNsPerSecond,
                            .updated_utc = utc};
  try {
    save_recovery(state, recovery_path());
    recovery_for_start_ = state.start_utc;
    last_recovery_write_ = now().mono;
  } catch (const std::exception& e) {
    std::fprintf(stderr, "mat-track: %s\n", e.what());
    status_ = "Warning: cannot write the recovery file.";
  }
}

void App::start_session() {
  const Now n = now();
  if (!timer_.start(selected_category_, n.mono, n.utc)) return;
  silence();
  banner_.clear();
  note_session_id_.reset();
  status_ = "Focus.";
  write_recovery(n.utc);
}

void App::pause_session() {
  const Now n = now();
  if (!timer_.pause(n.mono, n.utc)) return;
  if (timer_.state() == TimerState::Finished) {
    handle_finished();  // paused right at the target
    return;
  }
  status_ = "Paused.";
  write_recovery(n.utc);
}

void App::resume_session() {
  const Now n = now();
  if (!timer_.resume(n.mono, n.utc)) return;
  status_ = "Focus.";
  write_recovery(n.utc);
}

void App::stop_session() {
  const Now n = now();
  if (timer_.stop(n.mono, n.utc)) handle_finished();
}

void App::start_break() {
  if (!timer_.start_break(now().mono)) return;
  silence();
  status_ = timer_.on_long_break() ? "Long break." : "Break.";
}

void App::skip_break() {
  const Now n = now();
  if (!timer_.skip_break(n.mono, n.utc)) return;
  silence();
  status_ = "Break skipped.";
}

void App::ring(Alarm alarm) {
  alarm_ = alarm;
  if (config_.sound_enabled) {
    audio_.loop(alarm == Alarm::FocusDone ? Sound::Chime : Sound::BreakEnd);
  }
  SDL_FlashWindow(window_, SDL_FLASH_UNTIL_FOCUSED);
  if (config_.raise_on_alarm) {
    // Restore, raise and focus the window, and keep it above other windows
    // until the user responds.
    bring_to_front(window_);
    SDL_SetWindowAlwaysOnTop(window_, true);
    raised_for_alarm_ = true;
  }
}

void App::silence() {
  if (alarm_ == Alarm::None) return;
  alarm_ = Alarm::None;
  audio_.stop();
  SDL_FlashWindow(window_, SDL_FLASH_CANCEL);
  if (raised_for_alarm_) {
    SDL_SetWindowAlwaysOnTop(window_, false);
    raised_for_alarm_ = false;
  }
}

void App::handle_finished() {
  const FinishedSession f = *timer_.finished();
  const std::string logged = format_duration(f.session.duration_sec);

  if (f.loggable) {
    try {
      const Records before = records_;
      const std::int64_t id = db_->insert_session(f.session);
      reload_history();
      note_new_records(before);
      note_session_id_ = id;
      note_buf_.fill('\0');
    } catch (const std::exception& e) {
      // Keep the recovery file: it still holds this session.
      fail(std::string("Could not save the session. It is kept in the recovery file and will "
                       "be offered again at next start.\n\n") +
           e.what());
      status_ = "Error: session not saved.";
      timer_.acknowledge();
      return;
    }
    switch (f.reason) {
      case FinishReason::Completed:
        status_ = "Session complete: " + logged + " logged.";
        break;
      case FinishReason::Stopped:
        status_ = "Stopped early: " + logged + " logged.";
        break;
      case FinishReason::MaxDuration:
        status_ = "Auto-stopped at the " + format_duration(std::llround(config_.max_session_minutes * 60)) +
                  " limit: " + logged + " logged.";
        break;
    }
  } else {
    status_ = "Under " + std::to_string(config_.min_log_seconds) + "s: not logged.";
  }

  remove_recovery(recovery_path());
  recovery_for_start_ = -1;
  timer_.acknowledge();
  if (timer_.state() == TimerState::BreakReady) ring(Alarm::FocusDone);
}

// ---- crash recovery -----------------------------------------------------------

std::filesystem::path App::recovery_path() const { return data_dir_ / "active_session.json"; }

void App::check_recovery_file() {
  const auto path = recovery_path();
  std::optional<std::string> text;
  try {
    text = read_file(path);
  } catch (const std::exception& e) {
    std::fprintf(stderr, "mat-track: %s\n", e.what());
    return;
  }
  if (!text) return;

  const std::optional<RecoveryState> state = parse_recovery(*text);
  if (!state) {
    // Unreadable: set it aside rather than deleting it.
    std::error_code ec;
    std::filesystem::rename(path, std::filesystem::path(path).concat(".bad"), ec);
    std::fprintf(stderr, "mat-track: unreadable recovery file moved to %s.bad\n",
                 path.string().c_str());
    return;
  }
  if (state->focused_seconds < config_.min_log_seconds) {
    remove_recovery(path);  // too short to have been logged anyway
    return;
  }
  pending_recovery_ = state;
}

void App::save_recovered_session() {
  Session s = recovered_session(*pending_recovery_);
  // The category may have been removed since; fall back to the first one.
  if (!db_->category(s.category_id) && !categories_.empty()) {
    s.category_id = categories_.front().id;
  }
  try {
    const Records before = records_;
    db_->insert_session(s);
    reload_history();
    note_new_records(before);
    status_ = "Recovered session: " + format_duration(s.duration_sec) + " logged.";
  } catch (const std::exception& e) {
    fail(std::string("Could not save the recovered session; the recovery file is kept.\n\n") +
         e.what());
    pending_recovery_.reset();
    return;
  }
  remove_recovery(recovery_path());
  pending_recovery_.reset();
}

void App::discard_recovered_session() {
  remove_recovery(recovery_path());
  pending_recovery_.reset();
  status_ = "Interrupted session discarded.";
}

void App::render() {
  const ImGuiIO& io = ImGui::GetIO();
  SDL_SetRenderScale(renderer_, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
  const ImVec4 face = ImGui::ColorConvertU32ToFloat4(ui::palette().face);
  SDL_SetRenderDrawColorFloat(renderer_, face.x, face.y, face.z, 1.0f);
  SDL_RenderClear(renderer_);
  ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer_);
  SDL_RenderPresent(renderer_);
}

}  // namespace mattrack
