#include "core/goals.hpp"

#include <algorithm>

#include "core/db.hpp"

namespace mattrack {

GoalHistory::GoalHistory(std::vector<GoalChange> changes, int fallback)
    : changes_(std::move(changes)), fallback_(fallback) {
  std::sort(changes_.begin(), changes_.end(),
            [](const GoalChange& a, const GoalChange& b) { return a.from < b.from; });
}

int GoalHistory::on(Date day) const {
  if (changes_.empty()) return fallback_;
  // The last change on or before `day`; before the first one, the first.
  const auto it = std::upper_bound(changes_.begin(), changes_.end(), day,
                                   [](Date d, const GoalChange& c) { return d < c.from; });
  return it == changes_.begin() ? changes_.front().minutes : std::prev(it)->minutes;
}

int GoalHistory::latest() const { return changes_.empty() ? fallback_ : changes_.back().minutes; }

void sync_goal(Db& db, Date today, int goal_minutes) {
  const auto changes = db.goal_changes();
  if (changes.empty()) {
    // "Since forever": earlier than any study day.
    constexpr Date kBeginning{std::chrono::year{1970}, std::chrono::January, std::chrono::day{1}};
    db.set_goal_from(kBeginning, goal_minutes);
  } else if (GoalHistory(changes, 0).latest() != goal_minutes) {
    db.set_goal_from(today, goal_minutes);
  }
}

}  // namespace mattrack
