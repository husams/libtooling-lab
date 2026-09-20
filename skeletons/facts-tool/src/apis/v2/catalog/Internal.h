#pragma once
#include "apis/v2/catalog/Catalog.h"
#include "storage/catalog/Repository.h"
#include "storage/catalog/Component.h"
#include "storage/catalog/Directory.h"
#include "storage/catalog/File.h"
#include "tooling/StoredCompilationReader.h"
#include <algorithm>
#include <set>

namespace facts::apis::v2::catalog {
namespace native = ::facts::catalog;
using Database = native::Database;
template<class T> using Result = domain::Result<T>;
inline domain::Error invalid(std::string message) {
  return {422, "invalid_request", std::move(message)};
}
inline domain::Error conflict(std::string message) {
  return {409, "resource_conflict", std::move(message)};
}
template<class T> Result<T> lift(native::Result<T> value) {
  return std::move(value).transform_error([](const std::string &message) {
    if (message.find("not found") != std::string::npos)
      return domain::Error{404, "resource_not_found", message};
    if (message.find("SQL") != std::string::npos ||
        message.find("database is locked") != std::string::npos)
      return domain::Error{503, "catalog_unavailable", message};
    return conflict(message);
  });
}
Result<void> fields(const Json &, std::initializer_list<std::string_view>);
Result<std::int64_t> identifier(std::string_view);
Result<std::string> text(const Json &, std::string_view, bool required = false,
                         bool nullable = false);
Result<Json> page(Json items, const Json &query);
Result<native::Repository> repository(Database &, std::int64_t);
Result<native::File> file(Database &, std::int64_t);
Result<Json> encode(Database &, const native::Repository &);
Json encode(const native::Component &);
Json encode(const native::Directory &);
Result<StoredCompilationSnapshot> compilationContext(Database &);
Result<Json> compilationCommand(const native::File &, const StoredCompilationSnapshot &);
Result<Json> encode(const native::File &, const StoredCompilationSnapshot &);
Result<Json> encode(Database &, const native::File &);
Result<Response> repositories(Database &, std::string_view, std::string_view,
                              const Json &, const Json &);
Result<Response> components(Database &, std::string_view, std::string_view,
                            const Json &, const Json &);
Result<Response> files(Database &, std::string_view, std::string_view,
                       const Json &, const Json &);
Result<Response> directories(Database &, std::string_view, std::string_view,
                             const Json &, const Json &);
inline Result<void> dependencies(std::int64_t count, const Json &query) {
  if (!count || query.value("cascade", "false") == "true") return {};
  return std::unexpected(conflict("Resource has dependent registrations; use cascade=true to remove them"));
}
inline Response modified(Json value, std::string_view resource, bool created) {
  auto location = "/api/v2/" + std::string(resource) + "/" + value.at("id").get<std::string>();
  return {created ? 201U : 200U, std::move(value), std::move(location), true};
}
}
