#pragma once

#include <chrono>

#include "core/db.hpp"

namespace mattrack {

constexpr int kDemoDays = 182;

// Fills `db` with ~6 months of synthetic sessions (study days before and
// including today, never after `now`) across a few demo categories, created
// if missing. Deterministic for a given seed. Returns the number inserted.
int seed_demo(Db& db, UnixSeconds now, const std::chrono::time_zone& tz, int rollover_hour,
              unsigned seed = 42);

}  // namespace mattrack
