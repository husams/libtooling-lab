#include "analysis/callgraph/CallGraphCoverage.h"

#include <chrono>
#include <cmath>
#include <filesystem>

namespace facts::callgraph {

std::string coverageFreshness(const CoverageReport &report,
                              const QueryNode &node) {
  const auto *file = findCoverageEvidenceFile(report, node);
  if (!file || !file->indexed)
    return "unknown";
  if (file->mtime) {
    std::error_code error;
    const auto modified = std::filesystem::last_write_time(file->path, error);
    if (!error) {
      const auto seconds =
          std::chrono::duration<double>(
              decltype(modified)::clock::to_sys(modified).time_since_epoch())
              .count();
      if (std::abs(seconds - *file->mtime) > 1.0)
        return "stale";
    }
  }
  return file->indexedAt.empty() ? "unknown" : "fresh";
}

} // namespace facts::callgraph
