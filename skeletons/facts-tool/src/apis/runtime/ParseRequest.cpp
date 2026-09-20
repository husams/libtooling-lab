#include "apis/runtime/Validation.h"

namespace facts::apis::runtime {
domain::Result<Request> parseRequest(std::string operation, const Json &body) {
  std::set<std::string> allowed{"file"};
  if (operation == "extract") allowed.insert("force");
  if (operation == "match") allowed.insert({"query", "traversal", "relation_kind", "capture_source"});
  if (auto valid = keys(body, allowed); !valid) return std::unexpected(valid.error());
  if (!body.contains("file"))
    return std::unexpected(domain::Error{400, "invalid_request", "file is required"});
  const auto &file = body["file"];
  if (auto valid = keys(file, {"path", "repo", "clone", "component"}); !valid)
    return std::unexpected(valid.error());
  auto path = text(file.value("path", Json()), "file.path");
  if (!path) return std::unexpected(path.error());
  Request request{std::move(operation), {*path, {}, {}, {}}, body};
  for (auto [name, target] : {std::pair{"repo", &request.file.repo},
      {"clone", &request.file.clone}, {"component", &request.file.component}}) {
    if (!file.contains(name)) continue;
    auto value = text(file[name], name);
    if (!value) return std::unexpected(value.error());
    *target = std::move(*value);
  }
  for (const auto *name : {"force", "capture_source"})
    if (body.contains(name) && !body[name].is_boolean())
      return std::unexpected(domain::Error{400, "invalid_request", std::string(name) + " must be boolean"});
  if (request.operation == "match") {
    auto query = text(body.value("query", Json()), "query", 262144);
    if (!query) return std::unexpected(query.error());
    for (const auto *name : {"traversal", "relation_kind"}) if (body.contains(name)) {
      auto value = text(body[name], name);
      if (!value) return std::unexpected(value.error());
    }
    if (body.contains("traversal") && body["traversal"] != "AsIs" &&
        body["traversal"] != "IgnoreUnlessSpelledInSource")
      return std::unexpected(domain::Error{400, "invalid_request", "Unknown traversal mode"});
  }
  return request;
}
}
