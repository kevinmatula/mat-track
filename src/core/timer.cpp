#include "core/timer.hpp"

#include <algorithm>
#include <cmath>

namespace mattrack {

namespace {

MonoNs minutes_to_ns(double minutes) {
  return static_cast<MonoNs>(std::llround(minutes * 60.0 * static_cast<double>(kNsPerSecond)));
}

}  // namespace

TimerSettings TimerSettings::from_config(const Config& c) {
  return TimerSettings{
      .focus = minutes_to_ns(c.focus_minutes),
      .short_break = minutes_to_ns(c.break_minutes),
      .long_break = minutes_to_ns(c.long_break_minutes),
      .long_break_every = c.long_break_every,
      .auto_start_next = c.auto_start_next,
      .min_log_seconds = c.min_log_seconds,
      .max_session = minutes_to_ns(c.max_session_minutes),
  };
}

Timer::Timer(TimerSettings settings) : settings_(settings) {
  settings_.long_break_every = std::max(1, settings_.long_break_every);
}

bool Timer::start(std::int64_t category_id, MonoNs now, UnixSeconds now_utc) {
  if (state_ != TimerState::Idle) return false;
  begin_focus(category_id, now, now_utc);
  return true;
}

void Timer::begin_focus(std::int64_t category_id, MonoNs now, UnixSeconds now_utc) {
  state_ = TimerState::Running;
  category_id_ = category_id;
  start_utc_ = now_utc;
  focused_ = 0;
  last_mono_ = now;
  last_utc_ = now_utc;
  suspend_detected_ = false;
  finished_.reset();
}

bool Timer::pause(MonoNs now, UnixSeconds now_utc) {
  if (state_ != TimerState::Running) return false;
  advance(now, now_utc);
  if (state_ == TimerState::Running) state_ = TimerState::Paused;
  return true;
}

bool Timer::resume(MonoNs now, UnixSeconds now_utc) {
  if (state_ != TimerState::Paused) return false;
  state_ = TimerState::Running;
  last_mono_ = now;
  last_utc_ = now_utc;
  return true;
}

bool Timer::stop(MonoNs now, UnixSeconds now_utc) {
  if (state_ != TimerState::Running && state_ != TimerState::Paused) return false;
  if (state_ == TimerState::Running) advance(now, now_utc);
  // advance() may already have finished the session at its target.
  if (state_ != TimerState::Finished) finish(FinishReason::Stopped, now_utc);
  return true;
}

bool Timer::acknowledge() {
  if (state_ != TimerState::Finished) return false;
  const bool completed = finished_->reason == FinishReason::Completed;
  finished_.reset();
  if (!completed) {
    state_ = TimerState::Idle;
    return true;
  }
  state_ = TimerState::BreakReady;
  long_break_ = completed_in_cycle_ >= settings_.long_break_every;
  break_elapsed_ = 0;
  return true;
}

bool Timer::start_break(MonoNs now) {
  if (state_ != TimerState::BreakReady) return false;
  state_ = TimerState::Break;
  break_start_ = now;
  break_elapsed_ = 0;
  return true;
}

bool Timer::skip_break(MonoNs now, UnixSeconds now_utc) {
  if (state_ != TimerState::Break && state_ != TimerState::BreakReady) return false;
  end_break(now, now_utc);
  return true;
}

void Timer::tick(MonoNs now, UnixSeconds now_utc) {
  if (state_ == TimerState::Running) {
    advance(now, now_utc);
  } else if (state_ == TimerState::Break) {
    break_elapsed_ = std::max<MonoNs>(0, now - break_start_);
    const MonoNs overshoot = -(remaining_break());
    if (overshoot >= 0) end_break(now - overshoot, now_utc - overshoot / kNsPerSecond);
  }
}

MonoNs Timer::remaining() const {
  switch (state_) {
    case TimerState::Running:
    case TimerState::Paused:
      return std::max<MonoNs>(0, settings_.focus - focused_);
    case TimerState::Break:
    case TimerState::BreakReady:
      return std::max<MonoNs>(0, remaining_break());
    default:
      return 0;
  }
}

void Timer::advance(MonoNs now, UnixSeconds now_utc) {
  const MonoNs mono_delta = std::max<MonoNs>(0, now - last_mono_);
  const UnixSeconds wall_delta = now_utc - last_utc_;
  if (wall_delta - mono_delta / kNsPerSecond > kSuspendThresholdSeconds) {
    suspend_detected_ = true;
  }
  focused_ += mono_delta;
  last_mono_ = now;
  last_utc_ = now_utc;

  if (focused_ >= settings_.focus) {
    // Overshoot comes only from tick granularity; credit exactly the target.
    const MonoNs overshoot = focused_ - settings_.focus;
    focused_ = settings_.focus;
    finish(FinishReason::Completed, now_utc - overshoot / kNsPerSecond);
  } else if (focused_ >= settings_.max_session) {
    focused_ = settings_.max_session;
    finish(FinishReason::MaxDuration, now_utc);
  }
}

void Timer::finish(FinishReason reason, UnixSeconds now_utc) {
  state_ = TimerState::Finished;
  if (reason == FinishReason::Completed) ++completed_in_cycle_;

  FinishedSession f;
  f.reason = reason;
  f.session.start_utc = start_utc_;
  // The wall clock may have jumped backwards; never end before the start.
  f.session.end_utc = std::max(now_utc, start_utc_);
  f.session.duration_sec = focused_ / kNsPerSecond;
  f.session.category_id = category_id_;
  f.session.completed = reason == FinishReason::Completed;
  f.loggable = f.session.completed || f.session.duration_sec >= settings_.min_log_seconds;
  finished_ = f;
}

MonoNs Timer::remaining_break() const {
  return (long_break_ ? settings_.long_break : settings_.short_break) - break_elapsed_;
}

void Timer::end_break(MonoNs at, UnixSeconds at_utc) {
  if (long_break_) completed_in_cycle_ = 0;
  long_break_ = false;
  if (settings_.auto_start_next) {
    begin_focus(category_id_, at, at_utc);
  } else {
    state_ = TimerState::Idle;
  }
}

}  // namespace mattrack
