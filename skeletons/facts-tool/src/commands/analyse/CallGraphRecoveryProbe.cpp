#include "commands/analyse/CallGraphRecoveryInternal.h"
#include "commands/analyse/RecoveryScan.h"

namespace facts::commands {
std::expected<RecoveryProbeResult, std::string>
probeRecoveryCandidate(const RecoveryContext &context,
                       const RecoveryCandidate &candidate) {
  const auto found = context.scans.find(candidate.entry.tuFileId);
  if (found == context.scans.end())
    return std::unexpected("candidate has no frontend evidence");
  const auto &scan = *found->second;
  if (!scan.error.empty())
    return std::unexpected(scan.error);
  RecoveryProbeResult result{scan.status, {}};
  for (const auto &usr : candidate.entry.relatedUsrs)
    if (scan.facts.definitions.contains(usr))
      result.matched.insert(usr);
  return result;
}
} // namespace facts::commands
