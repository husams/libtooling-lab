#include "commands/ExtractionFreshness.h"

#include <cassert>
#include <optional>
#include <string>

namespace {

using facts::FileIndexState;
using facts::commands::FreshnessObservation;
using facts::commands::isUpToDate;

FileIndexState baseState() {
  FileIndexState state;
  state.indexed = true;
  state.indexedAt = "2026-09-12T10:00:00Z";
  state.mtime = 100.0;
  state.factsDb = "/tmp/facts.sqlite";
  state.gitCommit = std::string(40, 'a');
  return state;
}

FreshnessObservation baseObservation() {
  FreshnessObservation observation;
  observation.mtime = 100.0;
  observation.gitCommit = std::string(40, 'a');
  observation.factsDb = "/tmp/facts.sqlite";
  return observation;
}

} // namespace

int main() {
  // Every rule holding: up to date.
  assert(isUpToDate(baseState(), baseObservation()));

  // Rule 1: not indexed at all.
  {
    auto state = baseState();
    state.indexed = false;
    assert(!isUpToDate(state, baseObservation()));
  }
  // Rule 1: no recorded facts database.
  {
    auto state = baseState();
    state.factsDb.clear();
    assert(!isUpToDate(state, baseObservation()));
  }
  // Rule 1: recorded facts database differs from this run's output.
  {
    auto state = baseState();
    state.factsDb = "/tmp/other.sqlite";
    assert(!isUpToDate(state, baseObservation()));
  }

  // Rule 2: both NULL counts as equal.
  {
    auto state = baseState();
    state.gitCommit = std::nullopt;
    auto observation = baseObservation();
    observation.gitCommit = std::nullopt;
    assert(isUpToDate(state, observation));
  }
  // Rule 2: recorded NULL, current has a commit.
  {
    auto state = baseState();
    state.gitCommit = std::nullopt;
    assert(!isUpToDate(state, baseObservation()));
  }
  // Rule 2: recorded has a commit, current is NULL (the file left its repo).
  {
    auto observation = baseObservation();
    observation.gitCommit = std::nullopt;
    assert(!isUpToDate(baseState(), observation));
  }
  // Rule 2: different commits.
  {
    auto observation = baseObservation();
    observation.gitCommit = std::string(40, 'b');
    assert(!isUpToDate(baseState(), observation));
  }

  // Rule 3: the file could not be stat'd this run.
  {
    auto observation = baseObservation();
    observation.mtime = std::nullopt;
    assert(!isUpToDate(baseState(), observation));
  }
  // Rule 3: no recorded mtime to compare against.
  {
    auto state = baseState();
    state.mtime = std::nullopt;
    assert(!isUpToDate(state, baseObservation()));
  }
  // Rule 3: current mtime is later than recorded -- a real edit.
  {
    auto observation = baseObservation();
    observation.mtime = 200.0;
    assert(!isUpToDate(baseState(), observation));
  }
  // Rule 3: current mtime is earlier than recorded. There is no "not newer
  // than" allowance -- any difference, in either direction, is stale, so a
  // source whose mtime moves into the future is stale exactly once (the run
  // that observes the jump), not permanently: the next extraction records
  // that later mtime, and a subsequent unmodified check compares against
  // it exactly like any other recorded value.
  {
    auto observation = baseObservation();
    observation.mtime = 50.0;
    assert(!isUpToDate(baseState(), observation));
  }
  // Rule 3: an exact match despite carrying a fractional second is fresh --
  // indexed_at (whole seconds only) plays no part in this decision at all,
  // unlike the recorded mtime, which is compared at full precision.
  {
    auto state = baseState();
    state.mtime = 100.25;
    auto observation = baseObservation();
    observation.mtime = 100.25;
    assert(isUpToDate(state, observation));
  }
  // Rule 3: a same-second edit -- two fractional-second mtimes close enough
  // to share a floored indexed_at, but not exactly equal -- is still caught,
  // because the comparison is exact equality, not a coarser tolerance.
  {
    auto state = baseState();
    state.mtime = 100.25;
    auto observation = baseObservation();
    observation.mtime = 100.75;
    assert(!isUpToDate(state, observation));
  }
  // An unparseable or otherwise unusual indexed_at has no bearing on the
  // decision: only factsDb, git commit, and mtime matter.
  {
    auto state = baseState();
    state.indexedAt = "not-a-timestamp";
    assert(isUpToDate(state, baseObservation()));
  }
  return 0;
}
