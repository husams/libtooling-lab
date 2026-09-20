#include "apis/v2/catalog/Internal.h"

namespace facts::apis::v2::catalog {
namespace {
Result<native::Component> component(Database &database, std::int64_t id) {
  return lift(native::component(database, {.id = id}));
}
Result<Response> create(Database &database, const Json &body) {
  return fields(body, {"name", "path", "kind", "version", "repository"}).and_then([&]() -> Result<Response> {
    auto name = text(body, "name", true);
    auto path = text(body, "path", true);
    auto kind = body.contains("kind") ? text(body, "kind", true) : Result<std::string>{"repo"};
    auto version = text(body, "version", false, true);
    auto repo = text(body, "repository");
    for (const auto *value : {&name, &path, &kind, &version, &repo})
      if (!*value) return std::unexpected(value->error());
    if (*kind != "repo" && *kind != "external") return std::unexpected(invalid("kind must be repo or external"));
    return lift(native::addComponent(database, {*name, *path, *repo, *kind, *version, true}))
        .and_then([&] { return lift(native::component(database, {.name = *name})); })
        .and_then([&](const auto &value) {
          return lift(native::execute(database,
              "INSERT INTO directory(component_id,path) VALUES(?,'.') "
              "ON CONFLICT(component_id,path) DO NOTHING", value.value.id))
              .transform([&] { return value; });
        })
        .transform([](const auto &value) { return modified(encode(value), "components", true); });
  });
}
Result<Response> update(Database &database, const native::Component &value, const Json &body) {
  return fields(body, {"name", "version"}).and_then([&]() -> Result<Response> {
    auto name = body.contains("name") ? text(body, "name", true) : Result<std::string>{value.value.name};
    auto version = body.contains("version") ? text(body, "version", false, true)
                                            : Result<std::string>{value.value.version.value_or("")};
    if (!name) return std::unexpected(name.error());
    if (!version) return std::unexpected(version.error());
    return lift(native::components(database)).and_then([&](const auto &values) -> Result<void> {
      if (std::ranges::any_of(values, [&](const auto &item) { return item.value.id != value.value.id && item.value.name == *name; }))
        return std::unexpected(conflict("Component name already registered"));
      return lift(native::setVersion(database, value, *version));
    }).and_then([&] {
      return lift(native::execute(database, "UPDATE component SET name=? WHERE id=?", *name, value.value.id));
    }).and_then([&] {
      if (value.value.version.value_or("") == *version) return Result<void>{};
      return lift(native::execute(database,
          "INSERT OR IGNORE INTO api_index_invalidated_file(file_id) "
          "SELECT f.id FROM file f JOIN directory d ON d.id=f.directory_id WHERE d.component_id=?", value.value.id))
          .and_then([&] { return lift(native::execute(database,
              "UPDATE file SET indexed=0,indexed_at=NULL WHERE directory_id IN "
              "(SELECT id FROM directory WHERE component_id=?)", value.value.id)); });
    }).and_then([&] { return component(database, value.value.id); })
      .transform([](const auto &updated) { return modified(encode(updated), "components", false); });
  });
}
}
Result<Response> components(Database &database, std::string_view method, std::string_view id,
                            const Json &body, const Json &query) {
  if (id.empty() && method == "POST") return create(database, body);
  if (id.empty() && method == "GET")
    return lift(native::components(database)).and_then([&](const auto &values) {
      Json items = Json::array();
      for (const auto &value : values) {
        if (query.contains("repository") && value.repository != query.at("repository").get<std::string>()) continue;
        if (query.contains("component") && value.value.name != query.at("component").get<std::string>()) continue;
        items.push_back(encode(value));
      }
      return page(std::move(items), query);
    }).transform([](Json value) { return Response{200, std::move(value)}; });
  if (!id.empty() && (method == "GET" || method == "PATCH" || method == "DELETE"))
    return identifier(id).and_then([&](auto value) { return component(database, value); })
        .and_then([&](const auto &value) -> Result<Response> {
          if (method == "GET") return Response{200, encode(value)};
          if (method == "PATCH") return update(database, value, body);
          return lift(native::query(database, "SELECT count(*) FROM directory WHERE component_id=?",
              [](const storage::Row &row) { return row.integer(0); }, value.value.id))
              .and_then([&](const auto &counts) { return dependencies(counts.front(), query); })
              .and_then([&] { return lift(native::removeComponent(database, value.value.id)); })
              .transform([] { return Response{204, nullptr, {}, true}; });
        });
  return std::unexpected(domain::Error{405, "method_not_allowed", "Unsupported component operation"});
}
Result<Response> directories(Database &database, std::string_view method, std::string_view id,
                             const Json &, const Json &query) {
  if (id.empty() && method == "GET")
    return lift(native::directories(database, query.value("component", "")))
        .and_then([&](const auto &values) -> Result<Json> {
          auto owners = lift(native::components(database));
          if (!owners) return std::unexpected(owners.error());
          Json items = Json::array();
          for (const auto &value : values) {
            if (query.contains("repository") && !std::ranges::any_of(*owners, [&](const auto &owner) {
                  return owner.value.id == value.componentId && owner.repository == query.at("repository").get<std::string>();
                })) continue;
            items.push_back(encode(value));
          }
          return page(std::move(items), query);
        }).transform([](Json value) { return Response{200, std::move(value)}; });
  if (!id.empty() && (method == "GET" || method == "DELETE"))
    return identifier(id).and_then([&](auto value) { return lift(native::directory(database, {.id = value}, "")); })
        .and_then([&](const auto &value) -> Result<Response> {
          if (method == "GET") return Response{200, encode(value)};
          return dependencies(value.files, query)
              .and_then([&] { return lift(native::removeDirectory(database, value.id)); })
              .transform([] { return Response{204, nullptr, {}, true}; });
        });
  return std::unexpected(domain::Error{405, "method_not_allowed", "Unsupported directory operation"});
}
}
