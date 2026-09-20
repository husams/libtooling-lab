#include "apis/v2/catalog/Internal.h"
#include "storage/ActiveClone.h"

namespace facts::apis::v2::catalog {
namespace {
struct CloneInput {
  std::optional<std::int64_t> id;
  std::string path;
  std::string label;
};
Result<void> invalidate(Database &database, std::int64_t repository) {
  return lift(native::execute(database,
      "INSERT OR IGNORE INTO api_index_invalidated_file(file_id) "
      "SELECT f.id FROM file f JOIN directory d ON d.id=f.directory_id "
      "JOIN component c ON c.id=d.component_id WHERE c.repository_id=?", repository))
      .and_then([&] { return lift(native::execute(database,
          "UPDATE file SET indexed=0,indexed_at=NULL WHERE directory_id IN "
          "(SELECT d.id FROM directory d JOIN component c ON c.id=d.component_id WHERE c.repository_id=?)", repository)); });
}
Result<std::vector<CloneInput>> cloneInputs(const Json &input, bool creating) {
  if (!input.is_array() || input.empty())
    return std::unexpected(invalid("clones must be a nonempty array"));
  std::vector<CloneInput> result;
  std::set<std::string> paths, labels;
  std::set<std::int64_t> ids;
  for (const auto &value : input) {
    auto valid = creating ? fields(value, {"path", "label"}) : fields(value, {"id", "path", "label"});
    if (!valid) return std::unexpected(valid.error());
    auto path = text(value, "path", true);
    auto label = text(value, "label", false, true);
    if (!path) return std::unexpected(path.error());
    if (!label) return std::unexpected(label.error());
    auto normalized = lift(native::existingDirectory(*path));
    if (!normalized) return std::unexpected(invalid(normalized.error().message));
    CloneInput clone{std::nullopt, normalized->string(), *label};
    if (value.contains("id")) {
      auto token = text(value, "id", true);
      if (!token) return std::unexpected(token.error());
      auto id = identifier(*token);
      if (!id) return std::unexpected(id.error());
      clone.id = *id;
      if (!ids.insert(*id).second) return std::unexpected(invalid("Duplicate clone ID"));
    }
    if (!paths.insert(clone.path).second || (!clone.label.empty() && !labels.insert(clone.label).second))
      return std::unexpected(invalid("Clone paths and nonempty labels must be unique"));
    result.push_back(std::move(clone));
  }
  return result;
}
Result<std::int64_t> selectedClone(const Json &body, const native::Repository &repo,
                                  const std::vector<CloneInput> &clones) {
  const auto selected = body.contains("active_clone_id")
      ? text(body, "active_clone_id", true).and_then([](const auto &id) { return identifier(id); })
      : repo.activeCloneId ? Result<std::int64_t>{*repo.activeCloneId}
                           : Result<std::int64_t>{std::unexpected(conflict("Repository has no active clone"))};
  return selected.and_then([&](auto id) -> Result<std::int64_t> {
    if (!std::ranges::any_of(clones, [&](const auto &clone) { return clone.id == id; }))
      return std::unexpected(conflict("active_clone_id must identify a retained clone in this repository"));
    return id;
  });
}
Result<void> replaceClones(Database &database, const native::Repository &repo,
                           const std::vector<CloneInput> &inputs, std::int64_t active) {
  return lift(native::clones(database, repo.id)).and_then([&](const auto &existing) -> Result<void> {
    for (const auto &input : inputs) {
      if (input.id && !std::ranges::any_of(existing, [&](const auto &clone) { return clone.id == input.id; }))
        return std::unexpected(conflict("Clone ID does not belong to this repository"));
    }
    const bool activeChanged = repo.activeCloneId != active ||
        std::ranges::any_of(inputs, [&](const auto &input) {
          return input.id == active && std::ranges::any_of(existing, [&](const auto &clone) {
            return clone.id == active && clone.path != input.path;
          });
        });
    // Remove omitted registrations before adding replacements. All changes are
    // inside the caller's transaction, including the active-clone selection.
    for (const auto &clone : existing) {
      if (std::ranges::any_of(inputs, [&](const auto &value) { return value.id == clone.id; })) continue;
      auto removed = lift(native::execute(database, "DELETE FROM clone WHERE id=?", clone.id));
      if (!removed) return removed;
    }
    for (const auto &clone : inputs) {
      auto saved = clone.id
          ? lift(native::execute(database, "UPDATE clone SET path=?,label=NULLIF(?,'') WHERE id=? AND repository_id=?",
                                 clone.path, clone.label, *clone.id, repo.id))
          : lift(native::addClone(database, repo, clone.path, clone.label));
      if (!saved) return saved;
    }
    return storage::activateRegisteredClone(database, repo.id, active)
        .transform_error([](const auto &error) { return conflict(error.message()); })
        .and_then([&] { return activeChanged ? invalidate(database, repo.id) : Result<void>{}; });
  });
}
Result<Response> create(Database &database, const Json &body) {
  return fields(body, {"name", "remote_url", "clones"}).and_then([&]() -> Result<Response> {
    auto name = text(body, "name", true);
    auto remote = text(body, "remote_url", false, true);
    if (!name) return std::unexpected(name.error());
    if (!remote) return std::unexpected(remote.error());
    if (!body.contains("clones")) return std::unexpected(invalid("Missing field: clones"));
    return cloneInputs(body.at("clones"), true).and_then([&](const auto &clones) {
      return lift(native::addRepository(database, {*name, *remote, clones.front().path, clones.front().label}))
          .and_then([&](const auto &repo) -> Result<Json> {
            for (std::size_t index = 1; index < clones.size(); ++index) {
              auto added = lift(native::addClone(database, repo, clones[index].path, clones[index].label));
              if (!added) return std::unexpected(added.error());
            }
            return encode(database, repo);
          }).transform([](Json value) { return modified(std::move(value), "repositories", true); });
    });
  });
}
Result<Response> update(Database &database, const native::Repository &repo, const Json &body) {
  return fields(body, {"name", "remote_url", "clones", "active_clone_id"}).and_then([&]() -> Result<Response> {
    const auto name = body.contains("name") ? text(body, "name", true) : Result<std::string>{repo.name};
    const auto remote = body.contains("remote_url") ? text(body, "remote_url", false, true) : Result<std::string>{repo.remote};
    if (!name) return std::unexpected(name.error());
    if (!remote) return std::unexpected(remote.error());
    auto saved = lift(native::execute(database, "UPDATE repository SET name=?,remote_url=NULLIF(?,'') WHERE id=?", *name, *remote, repo.id));
    if (!saved) return std::unexpected(saved.error());
    if (body.contains("clones")) {
      auto result = cloneInputs(body.at("clones"), false).and_then([&](const auto &clones) {
        return selectedClone(body, repo, clones).and_then([&](auto active) {
          return replaceClones(database, repo, clones, active);
        });
      });
      if (!result) return std::unexpected(result.error());
    } else if (body.contains("active_clone_id")) {
      auto result = text(body, "active_clone_id", true).and_then([](const auto &id) { return identifier(id); })
          .and_then([&](auto id) {
            return storage::activateRegisteredClone(database, repo.id, id)
                .transform_error([](const auto &error) { return conflict("Invalid active clone: " + error.message()); })
                .and_then([&] { return id == repo.activeCloneId ? Result<void>{} : invalidate(database, repo.id); });
          });
      if (!result) return std::unexpected(result.error());
    }
    return repository(database, repo.id).and_then([&](const auto &value) { return encode(database, value); })
        .transform([](Json value) { return modified(std::move(value), "repositories", false); });
  });
}
}
Result<Response> repositories(Database &database, std::string_view method, std::string_view id,
                              const Json &body, const Json &query) {
  if (id.empty() && method == "POST") return create(database, body);
  if (id.empty() && method == "GET") {
    if (query.contains("component")) return std::unexpected(invalid("component is not a repository filter"));
    return lift(native::repositories(database)).and_then([&](const auto &values) -> Result<Json> {
      Json items = Json::array();
      for (const auto &value : values) {
        if (query.contains("repository") && value.name != query.at("repository").get<std::string>()) continue;
        auto item = encode(database, value);
        if (!item) return std::unexpected(item.error());
        items.push_back(std::move(*item));
      }
      return page(std::move(items), query);
    }).transform([](Json value) { return Response{200, std::move(value)}; });
  }
  if (!id.empty() && (method == "GET" || method == "PATCH" || method == "DELETE"))
    return identifier(id).and_then([&](auto value) { return repository(database, value); })
        .and_then([&](const auto &repo) -> Result<Response> {
          if (method == "GET") return encode(database, repo).transform([](Json value) { return Response{200, std::move(value)}; });
          if (method == "PATCH") return update(database, repo, body);
          return dependencies(repo.components, query).and_then([&] {
            return lift(native::removeRepository(database, repo, true));
          }).transform([] { return Response{204, nullptr, {}, true}; });
        });
  return std::unexpected(domain::Error{405, "method_not_allowed", "Unsupported repository operation"});
}
}
