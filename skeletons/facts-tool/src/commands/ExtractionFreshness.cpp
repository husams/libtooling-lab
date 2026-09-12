#include "commands/ExtractionFreshness.h"

#include "config/GitFileCommit.h"
#include "storage/FileManager.h"

#include <chrono>

namespace facts::commands {
namespace {

std::string normalizedPath(std::string_view path) {
  return std::filesystem::absolute(std::filesystem::path(path))
      .lexically_normal()
      .string();
}

// One file's up-to-date check plus the mtime observed while checking it, so
// partitionSources can hand that same observation to the marking stage
// instead of stat'ing the file a second time later.
struct FileObservation {
  bool upToDate;
  std::optional<double> mtime;
};

FileObservation checkFile(FileManager &files,
                          config::GitCommitResolver &resolver,
                          const std::string &file,
                          const std::string &normalizedOutput, bool force) {
  const auto absoluteFile = normalizedPath(file);
  const auto mtime = currentMtime(absoluteFile);
  if (force) {
    // --force already discards the freshness verdict below (every source
    // ends up stale regardless), so skip the registry lookup and git commit
    // resolution that only exist to compute it; only the mtime is needed,
    // to record what this run actually observed on disk.
    return {false, mtime};
  }
  auto state = files.indexState(file);
  FreshnessObservation observation;
  observation.factsDb = normalizedOutput;
  observation.mtime = mtime;
  observation.gitCommit = resolver.commitFor(absoluteFile);
  const bool upToDate = state && isUpToDate(*state, observation);
  return {upToDate, mtime};
}

} // namespace

std::optional<double> currentMtime(const std::filesystem::path &file) {
  std::error_code error;
  const auto writeTime = std::filesystem::last_write_time(file, error);
  if (error) {
    return std::nullopt;
  }
  const auto systemTime = std::chrono::file_clock::to_sys(writeTime);
  return std::chrono::duration<double>(systemTime.time_since_epoch()).count();
}

bool isUpToDate(const FileIndexState &state,
                const FreshnessObservation &observation) {
  // Rule 1: indexed, into the same facts database this run would write.
  if (!state.indexed || state.factsDb.empty() ||
      state.factsDb != observation.factsDb) {
    return false;
  }
  // Rule 2: the same git commit, where "both NULL" counts as the same.
  if (state.gitCommit != observation.gitCommit) {
    return false;
  }
  // Rule 3: the current mtime exactly matches the recorded one.
  // indexed_at is recorded metadata only and plays no part in this
  // decision: a source whose mtime moves into the future must not become
  // permanently stale (there is no "not newer than" clause below), and a
  // same-second edit on a coarse-timestamp filesystem is still caught
  // because the comparison is exact equality, not merely "not older".
  if (!observation.mtime || !state.mtime) {
    return false;
  }
  return *observation.mtime == *state.mtime;
}

PartitionedSources
partitionSources(FileManager &files, config::GitCommitResolver &resolver,
                 const std::vector<std::string> &sources,
                 const std::unordered_map<std::string, std::vector<std::string>>
                     &includedBySource,
                 const std::string &outputPath, bool force) {
  PartitionedSources result;
  const auto normalizedOutput = normalizedPath(outputPath);
  // Memoized per file path: a header shared by many translation units is
  // stat'd and queried against the registry exactly once for this call, no
  // matter how many TUs include it.
  std::unordered_map<std::string, FileObservation> observations;
  auto observe = [&](const std::string &file) -> const FileObservation & {
    auto found = observations.find(file);
    if (found == observations.end()) {
      found = observations
                  .emplace(file, checkFile(files, resolver, file,
                                           normalizedOutput, force))
                  .first;
    }
    return found->second;
  };
  for (const auto &source : sources) {
    // A source discoverIncludedFilesPerSource never saw (should not happen
    // once requireRegisteredSources has run over the same source list) is
    // conservatively checked as just itself.
    const std::vector<std::string> singleton{source};
    const auto included = includedBySource.find(source);
    const auto &closure =
        included != includedBySource.end() ? included->second : singleton;
    // --force still observes every file's mtime below (so the index state
    // this run records afterwards reflects reality rather than a stale
    // pre-parse stat); checkFile itself already returns upToDate=false for
    // every file once force is set, without doing the registry/git lookups
    // that verdict would otherwise need, so the fold below needs no special
    // case of its own.
    bool upToDate = true;
    for (const auto &file : closure) {
      const auto &observation = observe(file);
      if (observation.mtime) {
        result.observedMtime[file] = *observation.mtime;
      }
      upToDate = upToDate && observation.upToDate;
    }
    if (upToDate) {
      result.upToDate.push_back(source);
    } else {
      result.stale.push_back(source);
    }
  }
  return result;
}

} // namespace facts::commands
