#include "apis/operations/Details.h"
#include "storage/catalog/Database.h"

namespace facts::apis::operations {
domain::Result<void> completed(int status) {
  if (status != 0)
    return std::unexpected(operationError("native analysis failed"));
  return {};
}
domain::Result<void> recordFacts(const domain::Context &context,
                                 const domain::ResolvedFile &file,
                                 bool invalidateExtraction,
                                 std::span<const FileId> refreshed) {
  return catalog::open(context.configuration.database.string(), true)
      .and_then([&](catalog::Database database) {
        return database.write().transform_error([](auto error) { return error.message(); })
            .and_then([&](storage::Transaction transaction) {
              return catalog::execute(database,
                  "UPDATE file SET indexed=CASE WHEN ?3 OR facts_db IS NOT ?1 "
                  "THEN 0 ELSE indexed END, facts_db=?1 WHERE id=?2",
                  file.facts.string(), file.fileId, invalidateExtraction)
                  .and_then([&] {
                    return database.executeBulk("UPDATE file SET indexed=0 WHERE id=?1",
                        refreshed, [](sqlite3_stmt *statement, FileId id) {
                          return storage::bindInteger(statement, 1, id);
                        }).transform_error([](auto error) { return error.message(); })
                          .transform([](auto) {});
                  }).and_then([&] {
                    return transaction.commit()
                        .transform_error([](auto error) { return error.message(); });
                  });
            });
      }).transform_error(storageError);
}
nlohmann::json resultFile(const domain::ResolvedFile &file) {
  nlohmann::json clone = nullptr;
  if (file.clone)
    clone = file.clone->label.empty() ? file.clone->path : file.clone->label;
  return {{"id", file.fileId}, {"path", file.path.string()},
          {"repo", file.repository}, {"component", file.component},
          {"clone", std::move(clone)}};
}
nlohmann::json resultBase(std::string_view operation,
                          const domain::ResolvedFile &file) {
  return {{"operation", operation}, {"file", resultFile(file)},
          {"complete", true}, {"facts_committed", true}};
}
nlohmann::json jsonValue(const llvm::json::Value &value) {
  if (const auto *object = value.getAsObject()) {
    auto result = nlohmann::json::object();
    for (const auto &[key, field] : *object)
      result[key.str()] = jsonValue(field);
    return result;
  }
  if (const auto *array = value.getAsArray()) {
    auto result = nlohmann::json::array();
    for (const auto &field : *array) result.push_back(jsonValue(field));
    return result;
  }
  if (const auto string = value.getAsString()) return string->str();
  if (const auto boolean = value.getAsBoolean()) return *boolean;
  if (const auto integer = value.getAsInteger()) return *integer;
  if (const auto number = value.getAsNumber()) return *number;
  return nullptr;
}
}
