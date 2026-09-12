#pragma once

#include "commands/CallGraphInvalidation.h"
#include "commands/ConfigurationSupport.h"
#include "storage/catalog/Database.h"
#include <iostream>

namespace facts::commands {

// `coarseReset`: whether this catalog command, once it has actually
// invalidated a paired facts database's call-graph entries, should also
// reset every file row's index state (the structural repo/component/
// directory/clone mutations, where the whole catalog shape may have moved).
// Commands that scope their own invalidation to just the rows they touched
// (file set-option/clear-option) pass false and call resetIndexStateForIds
// themselves; commands that never invalidate anything meaningful for index
// state (file add/rm) also pass false and reset nothing at all.
template <typename Work>
catalog::Result<int> runCatalog(const std::string &path, bool writable,
                                Work work, bool create = false,
                                const std::string &selector = {},
                                const std::string &facts = {},
                                bool coarseReset = true) {
  auto resolved =
      loadConfiguration(path, selector, create || writable, writable);
  if (!resolved)
    return std::unexpected(resolved.error());
  create = create || (writable && resolved->generated);
  auto invalidate = [&]() -> catalog::Result<bool> {
    if (!writable)
      return false;
    return invalidateConfiguredCallGraphEntries(*resolved, facts);
  };
  return invalidate()
      .and_then([&](bool invalidatedFacts) -> catalog::Result<std::string> {
        return catalog::open(resolved->database.string(), writable, create)
            .and_then([&](catalog::Database database)
                          -> catalog::Result<std::string> {
              const auto operation = [&]() -> catalog::Result<std::string> {
                return work(database).and_then(
                    [&](std::string output) -> catalog::Result<std::string> {
                      if (!writable || !coarseReset || !invalidatedFacts)
                        return output;
                      // Same transaction as the mutation itself and gated
                      // on the exact condition that invalidated the paired
                      // facts database: no facts invalidated means nothing
                      // was extracted for this catalog yet, so there is no
                      // stale index state to reset.
                      return resetIndexState(database).transform(
                          [&] { return std::move(output); });
                    });
              };
              return writable ? catalog::transaction(database, operation)
                              : operation();
            });
      })
      .transform([](const std::string &output) {
        std::cout << output;
        return 0;
      });
}

} // namespace facts::commands
