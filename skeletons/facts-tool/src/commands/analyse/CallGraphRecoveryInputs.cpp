#include "commands/analyse/CallGraphRecoveryInternal.h"
#include "commands/analyse/RecoveryScan.h"

namespace facts::commands {
std::vector<recovery::RegisteredInput>
recoveryRegisteredInputs(RecoveryContext &context,
                         const RecoveryCandidate &candidate) {
  const auto id = candidate.entry.tuFileId;
  const auto scan = prepareRecoveryScan(context, candidate);
  context.inputClosures.erase(id);
  context.closureDigests.erase(id);
  if (scan->completeInputs && !scan->digest.empty()) {
    context.inputClosures[id] = scan->inputs;
    context.closureDigests[id] = scan->digest;
  }
  return scan->inputs;
}
} // namespace facts::commands
