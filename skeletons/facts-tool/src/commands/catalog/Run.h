#pragma once

#include "storage/catalog/Database.h"
#include <iostream>

namespace facts::commands {

template <typename Work>
catalog::Result<int> runCatalog(const std::string &path, bool writable,
                                Work work, bool create = false) {
  return catalog::open(path, writable, create)
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
