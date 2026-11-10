#pragma once

#include <vector>

#include "core/time_util.hpp"

namespace mattrack {

// A daily goal that applies from `from` onward (until the next change).
struct GoalChange {
  Date from;
  int minutes = 0;
  bool operator==(const GoalChange&) const = default;
};

// The daily goal over time, so changing the goal never regrades the past:
// each day is graded against the goal in effect on that day.
class GoalHistory {
 public:
  // A single goal for every day.
  explicit GoalHistory(int minutes = 120) : fallback_(minutes) {}
  // `changes` in any order. Days before the first change use the first
  // change's goal; with no changes, every day uses `fallback`.
  GoalHistory(std::vector<GoalChange> changes, int fallback);

  int on(Date day) const;
  // The newest goal (the one that applies today and later).
  int latest() const;

 private:
  std::vector<GoalChange> changes_;  // sorted by `from`
  int fallback_;
};

class Db;

// Keeps the database's goal history in step with the configured goal:
//  - no history yet: `goal_minutes` becomes the goal for all past days
//    (so existing history keeps the grades it had);
//  - the latest goal differs: `goal_minutes` applies from `today` on.
// Call at startup (config.toml may have been edited) and on every change.
void sync_goal(Db& db, Date today, int goal_minutes);

}  // namespace mattrack
