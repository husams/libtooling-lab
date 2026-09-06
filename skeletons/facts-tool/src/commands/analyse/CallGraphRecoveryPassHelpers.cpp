#include "commands/analyse/CallGraphRecoveryPassHelpers.h"

#include <string>
#include <utility>

namespace facts::commands {
namespace {
void recordFailure(RecoveryCandidate &candidate,
                   const recovery::AttemptKey &key,
                   const recovery::RequestedUsrs &requested,
                   const std::string &reason, const char *code,
                   RecoveryReport &report, recovery::AttemptCache &cache) {
  candidate.entry.reason = reason;
  (void)cache.record(key, requested, recovery::AttemptOutcome::failed,
                     {code, candidate.entry.reason});
  report.attempted.push_back(candidate.entry);
  report.failed.push_back(std::move(candidate.entry));
}
} // namespace

bool processRecoveryProbe(const RecoveryContext &context,
                          RecoveryCandidate &candidate,
                          const recovery::AttemptKey &key,
                          const recovery::RequestedUsrs &requested,
                          RecoveryReport &report,
                          recovery::AttemptCache &cache) {
  auto probe = probeRecoveryCandidate(context, candidate);
  if (!probe) {
    recordFailure(candidate, key, requested, probe.error(), "probe", report,
                  cache);
    return false;
  }
  if (probe->status != 0) {
    recordFailure(candidate, key, requested,
                  "probe failed with status " + std::to_string(probe->status),
                  "probe", report, cache);
    return false;
  }
  if (probe->matched.empty()) {
    candidate.entry.reason = "no_match";
    (void)cache.record(key, requested, recovery::AttemptOutcome::no_match,
                       {"no_match", "no matching definition"});
    report.attempted.push_back(candidate.entry);
    return false;
  }
  candidate.entry.relatedUsrs.assign(probe->matched.begin(),
                                     probe->matched.end());
  return true;
}
} // namespace facts::commands
