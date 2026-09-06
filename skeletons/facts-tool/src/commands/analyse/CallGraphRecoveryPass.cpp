#include "commands/analyse/CallGraphRecoveryInternal.h"

#include <string>

namespace facts::commands {
std::expected<bool, std::string> processRecoveryCandidates(
    const RecoveryContext &context, const cli::CallGraphOptions &options,
    std::vector<RecoveryCandidate> candidates, RecoveryReport &report,
    recovery::AttemptCache &cache, std::set<std::string> &preservedUsrs) {
  bool extracted = false;
  for (auto &candidate : candidates) {
    std::erase_if(candidate.entry.relatedUsrs,
                  [&](const auto &usr) { return preservedUsrs.contains(usr); });
    if (candidate.entry.relatedUsrs.empty())
      continue;
    recovery::AttemptInput input{
        {context.project, options.facts}, candidate.entry.tuFileId,
         candidate.entry.driver, candidate.entry.workingDirectory,
         candidate.entry.arguments, recoveryRegistryFingerprint(context),
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
    auto probe = probeRecoveryCandidate(context, candidate);
    if (!probe) {
      candidate.entry.reason = probe.error();
      (void)cache.record(*key, input.requested_usrs,
                         recovery::AttemptOutcome::failed,
                         {"probe", candidate.entry.reason});
      report.attempted.push_back(candidate.entry);
      report.failed.push_back(std::move(candidate.entry));
      continue;
    }
    if (probe->status != 0) {
      candidate.entry.reason = "probe failed with status " +
                               std::to_string(probe->status);
      (void)cache.record(*key, input.requested_usrs,
                         recovery::AttemptOutcome::failed,
                         {"probe", candidate.entry.reason});
      report.attempted.push_back(candidate.entry);
      report.failed.push_back(std::move(candidate.entry));
      continue;
    }
    if (probe->matched.empty()) {
      candidate.entry.reason = "no_match";
      (void)cache.record(*key, input.requested_usrs,
                         recovery::AttemptOutcome::no_match,
                         {"no_match", "no matching definition"});
      report.attempted.push_back(candidate.entry);
      continue;
    }
    if (probe->status == 0)
      candidate.entry.relatedUsrs.assign(probe->matched.begin(),
                                         probe->matched.end());
    auto attempt = candidate.entry;
    auto executed = extractRecoveryCandidate(context, options, candidate);
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
    (void)cache.record(*key, input.requested_usrs,
                       executed->succeeded ? recovery::AttemptOutcome::succeeded
                                           : recovery::AttemptOutcome::failed,
                       {executed->succeeded ? "succeeded" : "failed",
                        attempt.reason});
    report.attempted.push_back(attempt);
    if (!executed->succeeded)
      report.failed.push_back(std::move(attempt));
    else {
      preservedUsrs.insert(candidate.entry.relatedUsrs.begin(),
                           candidate.entry.relatedUsrs.end());
      return true;
    }
  }
  return extracted;
}

} // namespace facts::commands
