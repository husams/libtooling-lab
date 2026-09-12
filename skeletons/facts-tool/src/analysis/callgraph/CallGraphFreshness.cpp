#include "analysis/callgraph/CallGraphCoverage.h"

#include <chrono>
#include <filesystem>

namespace facts::callgraph {

bool coverageFileMtimeDrifted(const CoverageFile &file) {
  if (!file.indexed || !file.mtime)
    return false;
  std::error_code error;
  const auto modified = std::filesystem::last_write_time(file.path, error);
  if (error)
    return false;
  const auto seconds =
      std::chrono::duration<double>(
          decltype(modified)::clock::to_sys(modified).time_since_epoch())
          .count();
  // Exact comparison, not a coarse tolerance: a real edit changes mtime by
  // some fractional second almost everywhere, and a same-second edit is
  // exactly the case this needs to catch, not tolerate away.
  return seconds != *file.mtime;
}

std::string coverageFreshness(const CoverageReport &report,
                              const QueryNode &node) {
  const auto *file = findCoverageEvidenceFile(report, node);
  if (!file || !file->indexed)
    return "unknown";
  if (coverageFileMtimeDrifted(*file))
    return "stale";
  return file->indexedAt.empty() ? "unknown" : "fresh";
}

} // namespace facts::callgraph
