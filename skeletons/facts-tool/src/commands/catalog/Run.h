#pragma once

#include "storage/catalog/Database.h"
#include "commands/ConfigurationSupport.h"
#include <iostream>

namespace facts::commands {

template <typename Work>
catalog::Result<int> runCatalog(const std::string &path, bool writable,
                                Work work, bool create = false,
                                const std::string &selector = {}) {
  auto resolved = loadConfiguration(path, selector, create || writable);
  if (!resolved) return std::unexpected(resolved.error());
  create = create || (writable && resolved->generated);
  return catalog::open(resolved->database.string(), writable, create)
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
