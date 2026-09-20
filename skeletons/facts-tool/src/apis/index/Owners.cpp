#include "apis/index/Internal.h"

namespace facts::apis::index {
Result<void> prepareOwners(Database &database, const std::filesystem::path &base) {
  try {
    for (const auto &row : database.rows(
             "SELECT id,facts_db FROM file WHERE coalesce(facts_db,'')<>''")) {
      std::filesystem::path path = row.string(1);
      if (path.is_relative()) path = base / path;
      std::error_code error;
      path = std::filesystem::weakly_canonical(path, error);
      if (error) return std::unexpected("cannot resolve facts owner: " + error.message());
      auto result = catalog::execute(database,
          "INSERT INTO temp.api_symbol_owner(file_id,facts_db) VALUES(?,?)",
          row.integer(0), path.string());
      if (!result) return result;
    }
    return {};
  } catch (const storage::QueryError &error) {
    return std::unexpected(error.what());
  }
}
}
