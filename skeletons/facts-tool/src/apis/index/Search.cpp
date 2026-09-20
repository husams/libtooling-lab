#include "apis/index/Internal.h"

namespace facts::apis::index {
Result<Page> search(const std::filesystem::path &project, const Query &query) {
  return validate(query)
      .and_then([&] { return decodeCursor(query); })
      .and_then([&](const Position &position) {
        return catalog::open(project.string(), false)
            .and_then([&](Database database) {
              return database.read()
                  .transform_error([&](auto) { return catalog::databaseError(database); })
                  .and_then([&](storage::Transaction transaction) {
                    return readPage(database, query, position);
                  });
            });
      });
}
}
