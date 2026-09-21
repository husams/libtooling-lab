#include "apis/index/Internal.h"

namespace facts::apis::index {
Result<std::vector<std::filesystem::path>> sources(Database &database, const std::filesystem::path &) {
  return catalog::query(database, "SELECT DISTINCT facts_db FROM temp.api_symbol_owner ORDER BY facts_db",
      [](const storage::Row &row) { return std::filesystem::path(row.string(0)); });
}
}
