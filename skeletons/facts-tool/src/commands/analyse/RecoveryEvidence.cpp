#include "commands/analyse/RecoveryEvidence.h"

#include "storage/FactStore.h"

#include <algorithm>

namespace facts::commands {

RecoveryEvidence makeRecoveryEvidence(std::span<const SymbolId> symbols) {
  RecoveryEvidence evidence;
  evidence.entries.reserve(symbols.size());
  for (const auto symbol : symbols)
    if (symbol != SymbolId{})
      evidence.entries.push_back({symbol, symbol});
  std::ranges::sort(evidence.entries, {}, &CallGraphEntry::symbolId);
  evidence.entries.erase(
      std::ranges::unique(evidence.entries, {}, &CallGraphEntry::symbolId)
          .begin(),
      evidence.entries.end());
  return evidence;
}

std::expected<void, std::error_code>
publishRecoveryEvidence(FactStore &store, const RecoveryEvidence &evidence) {
  return store.addCallGraphEntries(evidence.entries);
}

} // namespace facts::commands
