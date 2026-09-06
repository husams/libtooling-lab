#include "commands/analyse/RecoveryReuseReport.h"

#include "commands/analyse/CallGraphRecoverySelection.h"

namespace facts::commands {
std::vector<RecoveryEntry>
makeRecoveryReuseReport(const RecoveryContext &context,
                        const RecoveryReuseGroups &groups) {
  std::vector<RecoveryEntry> result;
  result.reserve(groups.size());
  for (const auto &[file, usrs] : groups)
    result.push_back(makeEntry(context, file, {usrs.begin(), usrs.end()},
                               "existing valid body and calls"));
  return result;
}
} // namespace facts::commands
