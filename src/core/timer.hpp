#pragma once

#include <cstdint>
#include <optional>

#include "core/config.hpp"
#include "core/models.hpp"

namespace mattrack {

// Monotonic clock reading in nanoseconds (steady_clock in the app). On Linux
// it does not advance during suspend, so sleep is never credited as focus.
using MonoNs = std::int64_t;

constexpr MonoNs kNsPerSecond = 1'000'000'000;

enum class TimerState { Idle, Running, Paused, Break, Finished };

enum class FinishReason {
  Completed,    // ran to the target length
  Stopped,      // abandoned early by the user
  MaxDuration,  // hit max_session_minutes before the target
};

struct TimerSettings {
  MonoNs focus = 50 * 60 * kNsPerSecond;
  MonoNs short_break = 10 * 60 * kNsPerSecond;
  MonoNs long_break = 30 * 60 * kNsPerSecond;
  int long_break_every = 4;
  bool auto_start_next = false;
  int min_log_seconds = 60;
  MonoNs max_session = 240 * 60 * kNsPerSecond;

  static TimerSettings from_config(const Config& config);
};

// Result of a focus session, available while the timer is Finished.
struct FinishedSession {
  Session session;  // id = 0; ready to insert
  FinishReason reason = FinishReason::Stopped;
  // False for abandoned sessions shorter than min_log_seconds. Completed
  // sessions are always loggable, even if the target is very short.
  bool loggable = false;
};

// Pomodoro state machine. Pure: never reads a clock. Every command takes the
// current monotonic time, and the current UTC wall time where it is needed
// for start_utc / end_utc. Queries reflect the most recent command or tick().
//
//   Idle --start--> Running <--pause/resume--> Paused
//   Running --target reached--> Finished (Completed)
//   Running/Paused --stop--> Finished (Stopped)
//   Finished --acknowledge--> Break (after Completed) or Idle
//   Break --ends or skip_break--> Idle, or Running if auto_start_next
class Timer {
 public:
  explicit Timer(TimerSettings settings);

  // Each command returns false (and does nothing) if not valid in the
  // current state.
  bool start(std::int64_t category_id, MonoNs now, UnixSeconds now_utc);
  bool pause(MonoNs now, UnixSeconds now_utc);
  bool resume(MonoNs now, UnixSeconds now_utc);
  bool stop(MonoNs now, UnixSeconds now_utc);
  // Leaves Finished once the app has handled finished() (logged or not).
  // The break, if any, is timed from the moment the session finished.
  bool acknowledge(MonoNs now, UnixSeconds now_utc);
  bool skip_break(MonoNs now, UnixSeconds now_utc);

  // Advances Running focus time and the Break countdown; may finish the
  // session or end the break.
  void tick(MonoNs now, UnixSeconds now_utc);

  TimerState state() const { return state_; }
  const std::optional<FinishedSession>& finished() const { return finished_; }

  // Focus session (Running / Paused / Finished).
  std::int64_t category_id() const { return category_id_; }
  UnixSeconds start_utc() const { return start_utc_; }
  MonoNs focused() const { return focused_; }
  MonoNs focus_target() const { return settings_.focus; }

  // Time left on the focus session (Running / Paused) or on the break.
  MonoNs remaining() const;
  bool on_long_break() const { return long_break_; }

  // Completed focus sessions in the current cycle; resets after a long break.
  int completed_in_cycle() const { return completed_in_cycle_; }
  int long_break_every() const { return settings_.long_break_every; }

  // Set when wall time advanced much more than monotonic time while a
  // session was active (system suspend). The gap was not counted.
  bool suspend_detected() const { return suspend_detected_; }
  void clear_suspend_notice() { suspend_detected_ = false; }

  static constexpr UnixSeconds kSuspendThresholdSeconds = 60;

 private:
  // Credits focus time up to `now` and finishes on target / max duration.
  void advance(MonoNs now, UnixSeconds now_utc);
  void finish(FinishReason reason, MonoNs now, UnixSeconds now_utc);
  // `at` / `at_utc`: when the break ended (start time of an auto-started session).
  void end_break(MonoNs at, UnixSeconds at_utc);
  MonoNs remaining_break() const;  // negative once overdue
  void begin_focus(std::int64_t category_id, MonoNs now, UnixSeconds now_utc);

  TimerSettings settings_;
  TimerState state_ = TimerState::Idle;

  std::int64_t category_id_ = 0;
  UnixSeconds start_utc_ = 0;
  MonoNs focused_ = 0;
  MonoNs last_mono_ = 0;       // last time focus was credited
  UnixSeconds last_utc_ = 0;   // wall time at last_mono_, for suspend detection
  bool suspend_detected_ = false;

  std::optional<FinishedSession> finished_;
  MonoNs finished_mono_ = 0;

  MonoNs break_start_ = 0;
  MonoNs break_elapsed_ = 0;
  bool long_break_ = false;
  int completed_in_cycle_ = 0;
};

}  // namespace mattrack
