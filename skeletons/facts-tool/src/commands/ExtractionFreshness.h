#pragma once

#include "commands/IncludedFiles.h"
#include "storage/FileIndexState.h"

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace facts {
class FileManager;
}

namespace facts::config {
class GitCommitResolver;
}

namespace facts::commands {

// What the filesystem and git say about a source right now, gathered fresh
// for one freshness decision.
struct FreshnessObservation {
  std::optional<double> mtime; // nullopt: the file could not be stat'd
  std::optional<std::string> gitCommit;
  std::string factsDb; // the current run's output path, normalized
};

// The seconds-since-epoch of `file`'s last write, or nullopt when it cannot
// be read. Shared between the freshness check and the "record index state"
// stage so both sides agree on exactly the same representation.
std::optional<double> currentMtime(const std::filesystem::path &file);

// Whether a previously recorded index state still matches what the
// filesystem and git say now. Every rule below must hold to answer true;
// the first one that fails answers false.
bool isUpToDate(const FileIndexState &state,
                const FreshnessObservation &observation);

struct PartitionedSources {
  std::vector<std::string> stale;
  std::vector<std::string> upToDate;
  // Every file (TU or header) this check actually stat'd, mapped to the
  // mtime observed at that moment. The "record index state" stage reuses
  // this instead of stat'ing again, so a file edited while a long Clang
  // parse is still running records the mtime of what was actually
  // extracted, not whatever is on disk once the parse finishes.
  std::unordered_map<std::string, double> observedMtime;
};

// Splits `sources` into what still needs extraction and what does not,
// preserving each source's original position within its partition. A TU is
// up to date only when it and every file in its own entry of
// `includedBySource` (its transitive include set, itself included) are
// individually up to date -- an edited header makes the including TU stale
// even though the TU's own file did not change. `force` makes every source
// stale without consulting the registry.
PartitionedSources
partitionSources(FileManager &files, config::GitCommitResolver &resolver,
                 const std::vector<std::string> &sources,
                 const std::unordered_map<std::string, std::vector<std::string>>
                     &includedBySource,
                 const std::string &outputPath, bool force);

} // namespace facts::commands
