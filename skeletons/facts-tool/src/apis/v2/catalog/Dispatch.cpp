#include "apis/v2/catalog/Internal.h"

namespace facts::apis::v2::catalog {
namespace {
Result<void> invalidatePublishedIndex(Database &database) {
  return lift(native::query(database,
      "SELECT count(*) FROM sqlite_master WHERE type='table' AND name IN "
      "('global_symbol_index','global_symbol_index_state')",
      [](const storage::Row &row) { return row.integer(0); }))
      .and_then([&](const auto &counts) -> Result<void> {
        if (counts.front() != 2) return {};
        return lift(native::execute(database, "DELETE FROM global_symbol_index "
            "WHERE file_id IN (SELECT file_id FROM api_index_invalidated_file)"))
            .and_then([&] { return lift(native::execute(database,
                "UPDATE global_symbol_index_state SET generation=generation+1 WHERE id=1")); });
      });
}
Result<Response> route(Database &database, std::string_view method,
                       std::string_view resource, std::string_view id,
                       const Json &body, const Json &query) {
  if (resource == "repositories") return repositories(database, method, id, body, query);
  if (resource == "components") return components(database, method, id, body, query);
  if (resource == "files") return files(database, method, id, body, query);
  if (resource == "directories") return directories(database, method, id, body, query);
  return std::unexpected(domain::Error{404, "resource_not_found", "Unknown catalog resource"});
}
Result<void> validate(std::string_view method, std::string_view id,
                      const Json &body, const Json &query) {
  auto valid = fields(query, {"limit", "cursor", "repository", "component", "cascade"});
  if (!valid) return valid;
  for (const auto &[name, value] : query.items()) {
    if (!value.is_string()) return std::unexpected(invalid("Query parameters must be strings"));
    const bool allowed = method == "GET" && id.empty() ? name != "cascade"
                         : method == "DELETE" ? name == "cascade" : false;
    if (!allowed) return std::unexpected(invalid("Unsupported query parameter: " + name));
  }
  if (query.contains("cascade") && query["cascade"] != "true" && query["cascade"] != "false")
    return std::unexpected(invalid("cascade must be true or false"));
  if ((method == "GET" || method == "DELETE") && !body.is_null() && !body.empty())
    return std::unexpected(invalid("GET and DELETE requests must not contain a body"));
  return {};
}
}
Result<Response> dispatch(const domain::Context &context, std::string_view method,
    std::string_view resource, std::string_view id, const Json &body, const Json &query) {
  try {
    return validate(method, id, body, query).and_then([&] {
      return lift(native::open(context.configuration.database.string(), method != "GET", method == "POST"));
    }).and_then([&](Database database) -> Result<Response> {
      if (method == "GET") return route(database, method, resource, id, body, query);
      return database.write().transform_error([](const auto &error) {
        return domain::Error{503, "catalog_unavailable", error.message()};
      }).and_then([&](storage::Transaction transaction) {
        return lift(native::execute(database, "CREATE TABLE IF NOT EXISTS "
            "api_index_invalidated_file(file_id INTEGER PRIMARY KEY)"))
            .and_then([&] { return route(database, method, resource, id, body, query); })
            .and_then([&](Response result) -> Result<Response> {
              // Compiler-option and display metadata edits do not remove file
              // identities. Keep a completed registry usable by extraction;
              // its include-discovery step still checks every visited identity.
              const bool preserveRegistry = method == "PATCH" &&
                  (resource == "files" ||
                   (resource == "repositories" && !body.contains("clones") &&
                    !body.contains("active_clone_id")) ||
                   (resource == "components" && !body.contains("version")));
              auto registry = preserveRegistry ? Result<void>{} :
                  lift(native::execute(database, "UPDATE project_registry SET complete=0 WHERE id=1"));
              return std::move(registry)
                  .and_then([&] { return invalidatePublishedIndex(database); })
                  .and_then([&] { return transaction.commit().transform_error([](const auto &error) {
                    return domain::Error{503, "catalog_unavailable", error.message()};
                  }); }).transform([&] { return std::move(result); });
            });
      });
    });
  } catch (const Json::exception &error) {
    return std::unexpected(invalid(error.what()));
  } catch (const std::exception &error) {
    return std::unexpected(domain::Error{500, "catalog_failure", error.what()});
  }
}
}
