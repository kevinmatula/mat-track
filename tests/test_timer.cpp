#include <catch2/catch_test_macros.hpp>

#include "core/timer.hpp"

using namespace mattrack;

namespace {

constexpr MonoNs kSec = kNsPerSecond;
constexpr MonoNs kMin = 60 * kSec;
constexpr std::int64_t kCat = 7;

// Fake clocks. Normally both advance together; suspend() advances only the
// wall clock, like a laptop sleeping (CLOCK_MONOTONIC stops during suspend).
struct Clock {
  MonoNs mono = 0;
  UnixSeconds wall_offset = 1'718'438'400;  // wall time when mono == 0

  UnixSeconds utc() const { return wall_offset + mono / kSec; }
  void pass(MonoNs d) { mono += d; }
  void suspend(UnixSeconds seconds) { wall_offset += seconds; }
};

TimerSettings settings() {
  TimerSettings s;
  s.focus = 50 * kMin;
  s.short_break = 10 * kMin;
  s.long_break = 30 * kMin;
  s.long_break_every = 4;
  s.min_log_seconds = 60;
  s.max_session = 240 * kMin;
  return s;
}

struct Fixture {
  Clock clock;
  Timer timer{settings()};
  explicit Fixture(TimerSettings s = settings()) : timer(s) {}

  bool start() { return timer.start(kCat, clock.mono, clock.utc()); }
  bool pause() { return timer.pause(clock.mono, clock.utc()); }
  bool resume() { return timer.resume(clock.mono, clock.utc()); }
  bool stop() { return timer.stop(clock.mono, clock.utc()); }
  bool acknowledge() { return timer.acknowledge(clock.mono, clock.utc()); }
  bool skip_break() { return timer.skip_break(clock.mono, clock.utc()); }
  void tick() { timer.tick(clock.mono, clock.utc()); }
  // Advance in 250ms ticks, like the app's event loop.
  void run_for(MonoNs d) {
    const MonoNs end = clock.mono + d;
    while (clock.mono < end) {
      clock.pass(std::min<MonoNs>(250'000'000, end - clock.mono));
      tick();
    }
  }
  void complete_one() {
    REQUIRE(start());
    run_for(50 * kMin);
    REQUIRE(timer.state() == TimerState::Finished);
    REQUIRE(acknowledge());
  }
};

}  // namespace

TEST_CASE("start moves Idle to Running; invalid commands are rejected", "[timer]") {
  Fixture f;
  CHECK(f.timer.state() == TimerState::Idle);
  CHECK_FALSE(f.pause());
  CHECK_FALSE(f.resume());
  CHECK_FALSE(f.stop());
  CHECK_FALSE(f.acknowledge());
  CHECK_FALSE(f.skip_break());

  const UnixSeconds start_utc = f.clock.utc();
  CHECK(f.start());
  CHECK(f.timer.state() == TimerState::Running);
  CHECK(f.timer.category_id() == kCat);
  CHECK(f.timer.start_utc() == start_utc);
  CHECK(f.timer.remaining() == 50 * kMin);
  CHECK_FALSE(f.start());
  CHECK_FALSE(f.resume());
}

TEST_CASE("focus time accumulates only while Running", "[timer]") {
  Fixture f;
  const UnixSeconds start_utc = f.clock.utc();
  f.start();
  f.run_for(10 * kMin);
  CHECK(f.pause());
  CHECK(f.timer.state() == TimerState::Paused);
  CHECK_FALSE(f.pause());

  f.run_for(5 * kMin);  // paused: no credit
  CHECK(f.timer.focused() == 10 * kMin);
  CHECK(f.timer.remaining() == 40 * kMin);

  CHECK(f.resume());
  f.run_for(5 * kMin);
  CHECK(f.timer.focused() == 15 * kMin);

  CHECK(f.stop());
  const auto& fin = f.timer.finished();
  REQUIRE(fin);
  CHECK(fin->session.duration_sec == 15 * 60);
  CHECK(fin->session.start_utc == start_utc);
  CHECK(fin->session.end_utc == start_utc + 20 * 60);  // wall time includes the pause
}

TEST_CASE("reaching the target completes the session", "[timer]") {
  Fixture f;
  const UnixSeconds start_utc = f.clock.utc();
  f.start();
  f.run_for(50 * kMin - kSec);
  CHECK(f.timer.state() == TimerState::Running);
  CHECK(f.timer.remaining() == kSec);
  f.run_for(kSec);

  CHECK(f.timer.state() == TimerState::Finished);
  const auto& fin = f.timer.finished();
  REQUIRE(fin);
  CHECK(fin->reason == FinishReason::Completed);
  CHECK(fin->loggable);
  CHECK(fin->session.completed);
  CHECK(fin->session.category_id == kCat);
  CHECK(fin->session.duration_sec == 50 * 60);
  CHECK(fin->session.end_utc == start_utc + 50 * 60);
  CHECK(f.timer.completed_in_cycle() == 1);
}

TEST_CASE("a late tick credits exactly the target", "[timer]") {
  Fixture f;
  const UnixSeconds start_utc = f.clock.utc();
  f.start();
  f.clock.pass(50 * kMin + 3 * kSec);  // e.g. the event loop stalled
  f.tick();
  REQUIRE(f.timer.state() == TimerState::Finished);
  CHECK(f.timer.finished()->session.duration_sec == 50 * 60);
  CHECK(f.timer.finished()->session.end_utc == start_utc + 50 * 60);
}

TEST_CASE("abandoned sessions: logged if long enough, not completed", "[timer]") {
  Fixture f;
  f.start();

  SECTION("10 minutes is logged as 10 minutes") {
    f.run_for(10 * kMin);
    CHECK(f.stop());
    const auto& fin = f.timer.finished();
    REQUIRE(fin);
    CHECK(fin->reason == FinishReason::Stopped);
    CHECK(fin->loggable);
    CHECK_FALSE(fin->session.completed);
    CHECK(fin->session.duration_sec == 600);
  }
  SECTION("30 seconds is discarded") {
    f.run_for(30 * kSec);
    f.stop();
    CHECK_FALSE(f.timer.finished()->loggable);
  }
  SECTION("exactly min_log_seconds is logged") {
    f.run_for(60 * kSec);
    f.stop();
    CHECK(f.timer.finished()->loggable);
  }
  SECTION("stopping while paused excludes the pause") {
    f.run_for(2 * kMin);
    f.pause();
    f.run_for(30 * kMin);
    CHECK(f.stop());
    CHECK(f.timer.finished()->session.duration_sec == 120);
  }
  SECTION("abandoned sessions do not advance the pomodoro cycle") {
    f.run_for(10 * kMin);
    f.stop();
    CHECK(f.timer.completed_in_cycle() == 0);
  }
}

TEST_CASE("completed sessions are logged even if shorter than min_log_seconds", "[timer]") {
  auto s = settings();
  s.focus = 30 * kSec;  // e.g. focus_minutes = 0.5 for manual testing
  Fixture f(s);
  f.start();
  f.run_for(30 * kSec);
  REQUIRE(f.timer.state() == TimerState::Finished);
  CHECK(f.timer.finished()->loggable);
  CHECK(f.timer.finished()->session.duration_sec == 30);
}

TEST_CASE("after a session: break if completed, idle if not", "[timer]") {
  Fixture f;
  f.start();

  SECTION("stopped -> Idle") {
    f.run_for(10 * kMin);
    f.stop();
    CHECK(f.acknowledge());
    CHECK(f.timer.state() == TimerState::Idle);
    CHECK_FALSE(f.timer.finished());
  }
  SECTION("completed -> short Break that counts down to Idle") {
    f.run_for(50 * kMin);
    CHECK(f.acknowledge());
    CHECK(f.timer.state() == TimerState::Break);
    CHECK_FALSE(f.timer.on_long_break());
    CHECK(f.timer.remaining() == 10 * kMin);
    CHECK_FALSE(f.stop());
    CHECK_FALSE(f.pause());

    f.run_for(4 * kMin);
    CHECK(f.timer.remaining() == 6 * kMin);
    f.run_for(6 * kMin);
    CHECK(f.timer.state() == TimerState::Idle);
  }
  SECTION("skip_break goes straight to Idle") {
    f.run_for(50 * kMin);
    f.acknowledge();
    f.run_for(kMin);
    CHECK(f.skip_break());
    CHECK(f.timer.state() == TimerState::Idle);
    CHECK_FALSE(f.skip_break());
  }
  SECTION("the break is timed from the finish, not from acknowledge") {
    f.run_for(50 * kMin);
    f.clock.pass(4 * kMin);  // the app was slow to acknowledge
    f.acknowledge();
    CHECK(f.timer.remaining() == 6 * kMin);
  }
  SECTION("a break that is already over on acknowledge ends immediately") {
    f.run_for(50 * kMin);
    f.clock.pass(11 * kMin);
    f.acknowledge();
    CHECK(f.timer.state() == TimerState::Idle);
  }
}

TEST_CASE("long break after every N completed sessions", "[timer]") {
  Fixture f;
  for (int i = 1; i <= 3; ++i) {
    f.complete_one();
    CHECK(f.timer.completed_in_cycle() == i);
    CHECK_FALSE(f.timer.on_long_break());
    f.skip_break();
  }

  // An abandoned session in between does not count.
  f.start();
  f.run_for(5 * kMin);
  f.stop();
  f.acknowledge();
  CHECK(f.timer.completed_in_cycle() == 3);

  f.complete_one();
  CHECK(f.timer.completed_in_cycle() == 4);
  CHECK(f.timer.on_long_break());
  CHECK(f.timer.remaining() == 30 * kMin);
  f.run_for(30 * kMin);
  CHECK(f.timer.state() == TimerState::Idle);
  CHECK(f.timer.completed_in_cycle() == 0);

  // Next cycle starts over with short breaks.
  f.complete_one();
  CHECK(f.timer.completed_in_cycle() == 1);
  CHECK_FALSE(f.timer.on_long_break());
}

TEST_CASE("skipping a long break still resets the cycle", "[timer]") {
  auto s = settings();
  s.long_break_every = 1;
  Fixture f(s);
  f.complete_one();
  CHECK(f.timer.on_long_break());
  f.skip_break();
  CHECK(f.timer.completed_in_cycle() == 0);
}

TEST_CASE("auto_start_next starts a new session when the break ends", "[timer]") {
  auto s = settings();
  s.auto_start_next = true;
  Fixture f(s);
  f.complete_one();

  SECTION("break runs out") {
    const UnixSeconds break_end_utc = f.clock.utc() + 10 * 60;
    f.clock.pass(10 * kMin + 2 * kSec);  // a late tick
    f.tick();
    CHECK(f.timer.state() == TimerState::Running);
    CHECK(f.timer.category_id() == kCat);
    CHECK(f.timer.start_utc() == break_end_utc);
    f.tick();
    CHECK(f.timer.focused() == 2 * kSec);  // credited from the break's end
  }
  SECTION("break skipped") {
    f.run_for(kMin);
    const UnixSeconds now = f.clock.utc();
    CHECK(f.skip_break());
    CHECK(f.timer.state() == TimerState::Running);
    CHECK(f.timer.start_utc() == now);
    CHECK(f.timer.focused() == 0);
  }
}

TEST_CASE("suspend is not credited and is reported", "[timer]") {
  Fixture f;
  f.start();
  f.run_for(5 * kMin);

  SECTION("a 2 hour sleep") {
    f.clock.suspend(2 * 3600);
    f.tick();
    CHECK(f.timer.suspend_detected());
    CHECK(f.timer.focused() == 5 * kMin);
    f.run_for(5 * kMin);
    f.stop();
    CHECK(f.timer.finished()->session.duration_sec == 10 * 60);

    f.timer.clear_suspend_notice();
    CHECK_FALSE(f.timer.suspend_detected());
  }
  SECTION("small wall-clock drift is not a suspend") {
    f.clock.suspend(30);
    f.tick();
    CHECK_FALSE(f.timer.suspend_detected());
  }
  SECTION("suspend while paused is ignored") {
    f.pause();
    f.clock.suspend(3600);
    f.resume();
    f.tick();
    CHECK_FALSE(f.timer.suspend_detected());
  }
}

TEST_CASE("wall clock jumping backwards never yields end before start", "[timer]") {
  Fixture f;
  const UnixSeconds start_utc = f.clock.utc();
  f.start();
  f.run_for(5 * kMin);
  f.clock.wall_offset -= 3600;
  f.stop();
  const auto& s = f.timer.finished()->session;
  CHECK(s.end_utc == start_utc);
  CHECK(s.duration_sec == 5 * 60);  // monotonic time is still right
}

TEST_CASE("max_session caps a session longer than the target", "[timer]") {
  auto s = settings();
  s.focus = 300 * kMin;
  s.max_session = 240 * kMin;
  Fixture f(s);
  f.start();
  f.clock.pass(241 * kMin);
  f.tick();
  REQUIRE(f.timer.state() == TimerState::Finished);
  const auto& fin = f.timer.finished();
  CHECK(fin->reason == FinishReason::MaxDuration);
  CHECK_FALSE(fin->session.completed);
  CHECK(fin->session.duration_sec == 240 * 60);
  CHECK(fin->loggable);
  f.acknowledge();
  CHECK(f.timer.state() == TimerState::Idle);
}

TEST_CASE("pause or stop exactly at the target completes the session", "[timer]") {
  Fixture f;
  f.start();
  f.clock.pass(50 * kMin);
  SECTION("pause") { CHECK(f.pause()); }
  SECTION("stop") { CHECK(f.stop()); }
  CHECK(f.timer.state() == TimerState::Finished);
  CHECK(f.timer.finished()->reason == FinishReason::Completed);
}

TEST_CASE("a new session starts fresh", "[timer]") {
  Fixture f;
  f.start();
  f.run_for(10 * kMin);
  f.clock.suspend(3600);
  f.stop();
  f.acknowledge();

  CHECK(f.timer.start(kCat + 1, f.clock.mono, f.clock.utc()));
  CHECK(f.timer.focused() == 0);
  CHECK(f.timer.category_id() == kCat + 1);
  CHECK_FALSE(f.timer.suspend_detected());
  CHECK_FALSE(f.timer.finished());
}

TEST_CASE("settings from config", "[timer]") {
  Config c;
  c.focus_minutes = 0.5;
  c.break_minutes = 0.25;
  c.long_break_every = 2;
  c.auto_start_next = true;
  const TimerSettings s = TimerSettings::from_config(c);
  CHECK(s.focus == 30 * kSec);
  CHECK(s.short_break == 15 * kSec);
  CHECK(s.long_break == 30 * kMin);
  CHECK(s.long_break_every == 2);
  CHECK(s.auto_start_next);
  CHECK(s.min_log_seconds == 60);
  CHECK(s.max_session == 240 * kMin);
}
