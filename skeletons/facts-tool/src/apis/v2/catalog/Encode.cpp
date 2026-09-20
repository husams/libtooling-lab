#include "apis/v2/catalog/Internal.h"

namespace facts::apis::v2::catalog {
namespace {
Json nullable(const std::string &value) { return value.empty() ? Json(nullptr) : Json(value); }
Json id(const std::optional<std::int64_t> &value) {
  return value ? Json(std::to_string(*value)) : Json(nullptr);
}
}
Result<Json> encode(Database &database, const native::Repository &value) {
  return lift(native::clones(database, value.id)).and_then([&](const auto &clones) -> Result<Json> {
    Json values = Json::array();
    for (const auto &clone : clones)
      values.push_back({{"id", std::to_string(clone.id)}, {"path", clone.path}, {"label", nullable(clone.label)}});
    auto counts = lift(native::query(database,
        "SELECT count(*),coalesce(sum(f.indexed!=0),0) FROM file f "
        "JOIN directory d ON d.id=f.directory_id JOIN component c ON c.id=d.component_id "
        "WHERE c.repository_id=? AND coalesce(f.driver,'')!=''",
        [](const storage::Row &row) { return std::pair{row.integer(0), row.integer(1)}; }, value.id));
    if (!counts) return std::unexpected(counts.error());
    return Json{{"id", std::to_string(value.id)}, {"name", value.name},
        {"kind", value.kind}, {"remote_url", nullable(value.remote)},
        {"active_clone_id", id(value.activeCloneId)}, {"clones", std::move(values)},
        {"component_count", value.components}, {"source_count", counts->front().first},
        {"indexed_source_count", counts->front().second}};
  });
}
Json encode(const native::Component &value) {
  return {{"id", std::to_string(value.value.id)}, {"name", value.value.name},
      {"path", value.value.path}, {"kind", value.value.kind},
      {"version", value.value.version ? Json(*value.value.version) : Json(nullptr)},
      {"repository_id", id(value.value.repositoryId)}, {"repository", nullable(value.repository)},
      {"file_count", value.files}};
}
Json encode(const native::Directory &value) {
  return {{"id", std::to_string(value.id)}, {"component_id", std::to_string(value.componentId)},
      {"component", value.component}, {"path", value.path}, {"file_count", value.files}};
}
Result<Json> encode(const native::File &value, const StoredCompilationSnapshot &context) {
  return lift(native::filePath(value)).and_then([&](const auto &path) -> Result<Json> {
    auto command = compilationCommand(value, context);
    if (!command) return std::unexpected(command.error());
    return Json{{"id", std::to_string(value.id)}, {"path", path.string()}, {"name", value.name},
        {"directory_id", std::to_string(value.directoryId)},
        {"component_id", std::to_string(value.component.id)}, {"component", value.componentName},
        {"repository_id", id(value.component.repositoryId)}, {"indexed", value.indexed},
        {"compilation_command", std::move(*command)}};
  });
}
Result<Json> encode(Database &database, const native::File &value) {
  return compilationContext(database).and_then([&](const auto &context) { return encode(value, context); });
}
}
