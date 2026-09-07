#pragma once

#include "commands/CallGraphInvalidation.h"
#include "commands/ConfigurationSupport.h"
#include "storage/catalog/Database.h"
#include <iostream>

namespace facts::commands {

template <typename Work>
catalog::Result<int> runCatalog(const std::string &path, bool writable,
                                Work work, bool create = false,
                                const std::string &selector = {},
                                const std::string &facts = {}) {
  auto resolved =
      loadConfiguration(path, selector, create || writable, writable);
  if (!resolved)
    return std::unexpected(resolved.error());
  create = create || (writable && resolved->generated);
  auto invalidate = [&]() -> catalog::Result<void> {
    if (!writable)
      return {};
    return invalidateConfiguredCallGraphEntries(*resolved, facts);
  };
  return invalidate()
      .and_then([&] {
        return catalog::open(resolved->database.string(), writable, create);
      })
      .and_then(
          [&](catalog::Database database) -> catalog::Result<std::string> {
            const auto operation = [&] { return work(database); };
            return writable ? catalog::transaction(database, operation)
                            : operation();
          })
      .transform([](const std::string &output) {
        std::cout << output;
        return 0;
      });
}

} // namespace facts::commands
