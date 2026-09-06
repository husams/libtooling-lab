#include "commands/analyse/CallGraphRecoveryInternal.h"
#include "commands/analyse/CallGraphRecoveryPassHelpers.h"
#include <algorithm>

namespace facts::commands {
std::expected<bool, std::string> processRecoveryCandidates(
    RecoveryContext &context, const cli::CallGraphOptions &options,
    std::vector<RecoveryCandidate> candidates, RecoveryReport &report,
    recovery::AttemptCache &cache, callgraph::QueryGraph &graph) {
  auto &preservedUsrs = context.preservedUsrs;
  for (auto &candidate : candidates) {
    std::erase_if(candidate.entry.relatedUsrs,
                  [&](const auto &usr) { return preservedUsrs.contains(usr); });
    if (candidate.entry.relatedUsrs.empty())
      continue;
    recovery::AttemptInput input{
        {context.project, options.facts},
        candidate.entry.tuFileId,
        candidate.entry.driver,
        candidate.entry.workingDirectory,
        candidate.entry.arguments,
        recoveryRegistryFingerprint(context),
        recoveryRegisteredInputs(context, candidate),
        recovery::RequestedUsrs(candidate.entry.relatedUsrs.begin(),
                                candidate.entry.relatedUsrs.end())};
    auto key = cache.build(input);
    if (!key) {
      candidate.entry.reason = key.error().message;
      report.attempted.push_back(candidate.entry);
      report.failed.push_back(std::move(candidate.entry));
      continue;
    }
    if (const auto previous = cache.find(*key, input.requested_usrs)) {
      candidate.entry.reason = "suppressed " + previous->diagnostic.code;
      report.suppressed.push_back(std::move(candidate.entry));
      continue;
    }
    if (!processRecoveryProbe(context, candidate, *key, input.requested_usrs,
                              report, cache))
      continue;
    if (auto valid =
            validateRecoveryEvidence(context, options, graph, candidate)) {
      for (auto &node : graph.nodes)
        if (std::ranges::contains(candidate.entry.relatedUsrs, node.usr)) {
          node.bodyEvidence = true;
          preservedUsrs.insert(node.usr);
          context.reusedUsrs.insert(node.usr);
          context.reusedOwners[node.usr] = candidate.entry.tuFileId;
        }
      continue;
    }
    std::vector<SymbolId> retainedIds;
    for (const auto &node : graph.nodes)
      if (preservedUsrs.contains(node.usr) && node.bodyEvidence)
        retainedIds.push_back(node.id);
    const auto retained = makeRecoveryEvidence(retainedIds);
    auto attempt = candidate.entry;
    auto executed =
        extractRecoveryCandidate(context, options, candidate, &retained);
    if (!executed) {
      attempt.reason = executed.error();
      (void)cache.record(*key, input.requested_usrs,
                         recovery::AttemptOutcome::failed,
                         {"extract", attempt.reason});
      report.attempted.push_back(attempt);
      report.failed.push_back(std::move(attempt));
      continue;
    }
    attempt.reason = executed->reason;
    (void)cache.record(
        *key, input.requested_usrs,
        executed->succeeded ? recovery::AttemptOutcome::succeeded
                            : recovery::AttemptOutcome::failed,
        {executed->succeeded ? "succeeded" : "failed", attempt.reason});
    report.attempted.push_back(attempt);
    if (!executed->succeeded)
      report.failed.push_back(std::move(attempt));
    else {
      preservedUsrs.insert(candidate.entry.relatedUsrs.begin(),
                           candidate.entry.relatedUsrs.end());
      return true;
    }
  }
  return false;
}

} // namespace facts::commands
