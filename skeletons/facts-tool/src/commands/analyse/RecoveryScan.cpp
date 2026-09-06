#include "commands/analyse/RecoveryScan.h"
#include "commands/analyse/CallGraphRecoveryInternal.h"
#include "commands/analyse/RecoveryCompilation.h"
#include <clang/Tooling/Tooling.h>

namespace facts::commands {
bool recoveryScanCurrent(RecoveryContext &context, const RecoveryScan &scan) {
  if (scan.registry != recoveryRegistryFingerprint(context) ||
      scan.availability != recoveryInputAvailability(context))
    return false;
  const auto digest = context.digests.digest(scan.inputs);
  return digest && *digest == scan.digest;
}

std::shared_ptr<RecoveryScan>
prepareRecoveryScan(RecoveryContext &context,
                    const RecoveryCandidate &candidate) {
  const auto id = candidate.entry.tuFileId;
  if (const auto found = context.scans.find(id);
      found != context.scans.end() &&
      recoveryScanCurrent(context, *found->second)) {
    collectRecoveryScanBodies(candidate, *found->second);
    return found->second;
  }
  auto scan = std::make_shared<RecoveryScan>();
  scan->registry = recoveryRegistryFingerprint(context);
  scan->availability = recoveryInputAvailability(context);
  if (candidate.entry.arguments.empty()) {
    scan->error = candidate.entry.reason;
  } else {
    RecoveryCompilation database(candidate);
    const std::vector<std::string> sources{candidate.source.string()};
    clang::tooling::ClangTool tool(database, sources);
    tool.clearArgumentsAdjusters();
    scan->status = tool.buildASTs(scan->units);
    if (scan->units.empty())
      scan->status = 1;
    for (const auto &unit : scan->units)
      if (unit->getDiagnostics().hasErrorOccurred())
        scan->status = 1;
  }
  collectRecoveryScanInputs(context, candidate, *scan);
  if (const auto digest = context.digests.digest(scan->inputs))
    scan->digest = *digest;
  collectRecoveryScanBodies(candidate, *scan);
  context.scans[id] = scan;
  return scan;
}
} // namespace facts::commands
