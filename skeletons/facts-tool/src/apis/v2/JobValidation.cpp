#include "apis/v2/JobServices.h"
#include "apis/runtime/Validation.h"
#include <set>
#include <charconv>

namespace facts::apis::v2::jobs {
namespace {
Result<void> strings(const Json &value, const std::set<std::string> &fields) {
  for (const auto &name : fields) if (value.contains(name)) {
    auto checked = runtime::text(value[name], name);
    if (!checked) return std::unexpected(invalid(checked.error().message));
  }
  return {};
}
Result<void> selectionKeys(const Json &selection, const std::set<std::string> &allowed,
                           const std::string &required) {
  return runtime::keys(selection, allowed).and_then([&]() -> Result<void> {
    if (!selection.contains(required))
      return std::unexpected(invalid("selection requires " + required));
    return strings(selection, {"type", "repository", "component", "path"});
  });
}
}
Result<void> validateSymbol(const Json &value) {
  return runtime::keys(value, {"qualified_name", "usr", "symbol_id", "repository"})
      .and_then([&]() -> Result<void> {
        const auto count = value.contains("qualified_name") + value.contains("usr") +
                           value.contains("symbol_id");
        if (count != 1) return std::unexpected(invalid(
            "symbol selection requires exactly one of qualified_name, usr, symbol_id"));
        return strings(value, {"qualified_name", "usr", "symbol_id", "repository"});
      });
}
Result<void> validateSelection(const Json &value) {
  if (!value.is_object() || !value.contains("type") || !value["type"].is_string())
    return std::unexpected(invalid("selection requires a type"));
  const auto type = value["type"].get<std::string>();
  if (type == "all") return runtime::keys(value, {"type"});
  if (type == "repository")
    return selectionKeys(value, {"type", "repository"}, "repository");
  if (type == "component")
    return selectionKeys(value, {"type", "component", "repository"}, "component");
  if (type == "directory")
    return selectionKeys(value, {"type", "path", "repository"}, "path");
  if (type != "files") return std::unexpected(invalid("unknown selection type"));
  return runtime::keys(value, {"type", "files"}).and_then([&]() -> Result<void> {
    if (!value.contains("files") || !value["files"].is_array() || value["files"].empty() ||
        value["files"].size() > 10000)
      return std::unexpected(invalid("files must contain between 1 and 10000 selectors"));
    for (const auto &file : value["files"]) {
      auto fields = runtime::keys(file, {"path", "file_id", "repository", "clone", "component"});
      if (!fields) return fields;
      if (file.contains("path") == file.contains("file_id"))
        return std::unexpected(invalid("file requires exactly one of path and file_id"));
      if (file.contains("file_id")) {
        if (!file["file_id"].is_string()) return std::unexpected(invalid("file_id must be a decimal string"));
        const auto id = file["file_id"].get<std::string>();
        std::int64_t parsed = 0;
        const auto converted = std::from_chars(id.data(), id.data() + id.size(), parsed);
        if (converted.ec != std::errc{} || converted.ptr != id.data() + id.size() || parsed < 1)
          return std::unexpected(invalid("file_id must be a positive decimal string"));
      }
      auto values = strings(file, {"path", "repository", "clone", "component"});
      if (!values) return values;
    }
    return {};
  });
}
}

namespace facts::apis::v2 {
domain::Result<runtime::Request> parseJobRequest(std::string operation,
                                                const nlohmann::json &body) {
  using namespace jobs;
  static const std::map<std::string, std::set<std::string>> fields{
      {"index", {}},
      {"extract", {"selection", "force", "continue_on_error"}},
      {"match", {"selection", "expression", "traversal", "capture_source", "relation_kind", "bindings", "continue_on_error"}},
      {"dependencies", {"selection", "continue_on_error"}},
      {"import", {"selection", "repository", "compilation_database"}},
      {"scan", {"selection"}},
      {"callgraphs", {"root", "target", "direction", "max_depth", "max_nodes", "max_edges", "time_limit_ms", "path_mode"}},
      {"variable-flow", {"selection", "function", "variable", "direction", "interprocedural", "max_call_depth"}}};
  auto allowed = fields.find(operation);
  if (allowed == fields.end())
    return std::unexpected(domain::Error{404, "resource_not_found", "Unknown analysis resource"});
  auto keys = runtime::keys(body, allowed->second);
  if (!keys) return std::unexpected(invalid(keys.error().message));
  for (const auto *key : {"force", "capture_source", "interprocedural", "continue_on_error"})
    if (body.contains(key) && !body[key].is_boolean())
      return std::unexpected(invalid(std::string(key) + " must be boolean"));
  for (const auto *key : {"max_depth", "max_call_depth", "max_nodes", "max_edges", "time_limit_ms"})
    if (body.contains(key) && (!body[key].is_number_integer() ||
        body[key].get<std::int64_t>() < 0 || body[key].get<std::int64_t>() > 2147483647 ||
        ((std::string_view(key) == "max_nodes" || std::string_view(key) == "max_edges" ||
          std::string_view(key) == "time_limit_ms") && body[key] == 0)))
      return std::unexpected(invalid(std::string(key) + " is outside its supported range"));
  if (body.contains("selection")) {
    auto selection = validateSelection(body["selection"]);
    if (!selection) return std::unexpected(invalid(selection.error().message));
    if ((operation == "scan" || operation == "import") &&
        body["selection"]["type"] != "all" && body["selection"]["type"] != "repository" &&
        body["selection"]["type"] != "directory")
      return std::unexpected(invalid("scan/import selection must be all, repository, or directory"));
  } else if (operation == "extract" || operation == "match" ||
             operation == "dependencies" || operation == "scan")
    return std::unexpected(invalid("selection is required"));
  for (const auto *key : {"expression", "traversal", "relation_kind", "repository", "compilation_database", "direction", "path_mode"})
    if (body.contains(key)) {
      auto value = runtime::text(body[key], key, std::string_view(key) == "expression" ? 65536 : 4096);
      if (!value) return std::unexpected(invalid(value.error().message));
    }
  if (body.contains("repository") && body.contains("selection") &&
      body["selection"].contains("repository") && body["repository"] != body["selection"]["repository"])
    return std::unexpected(invalid("repository and selection.repository must agree"));
  if (operation == "match" && !body.contains("expression"))
    return std::unexpected(invalid("expression is required"));
  if (body.contains("traversal") && body["traversal"] != "AsIs" &&
      body["traversal"] != "IgnoreUnlessSpelledInSource")
    return std::unexpected(invalid("traversal must be AsIs or IgnoreUnlessSpelledInSource"));
  if (body.contains("bindings")) {
    auto checked = runtime::keys(body["bindings"], {"source", "target", "site", "call", "callee"});
    if (!checked) return std::unexpected(invalid(checked.error().message));
    for (const auto &[key, value] : body["bindings"].items()) {
      auto binding = runtime::text(value, key);
      if (!binding) return std::unexpected(invalid(binding.error().message));
    }
  }
  if (operation == "callgraphs" || operation == "variable-flow") {
    const auto root = operation == "callgraphs" ? "root" : "function";
    if (!body.contains(root)) return std::unexpected(invalid(std::string(root) + " is required"));
    auto symbol = validateSymbol(body[root]);
    if (!symbol) return std::unexpected(invalid(symbol.error().message));
  }
  if (body.contains("target")) {
    auto target = validateSymbol(body["target"]);
    if (!target) return std::unexpected(invalid(target.error().message));
  }
  if (operation == "callgraphs") {
    if (body.contains("direction") && body["direction"] != "callers" && body["direction"] != "callees")
      return std::unexpected(invalid("direction must be callers or callees"));
    if (body.contains("path_mode") && (!body.contains("target") ||
        (body["path_mode"] != "shortest" && body["path_mode"] != "all_simple")))
      return std::unexpected(invalid("path_mode requires target and must be shortest or all_simple"));
  }
  if (operation == "variable-flow") {
    if (body.contains("direction") && body["direction"] != "forward")
      return std::unexpected(invalid("variable-flow currently supports forward tracking"));
    if (!body.contains("variable")) return std::unexpected(invalid("variable is required"));
    auto variable = runtime::keys(body["variable"], {"name", "declaration"});
    if (!variable || !body["variable"].contains("name"))
      return std::unexpected(invalid("variable requires name and optional declaration"));
    auto name = runtime::text(body["variable"]["name"], "variable.name");
    if (!name) return std::unexpected(invalid(name.error().message));
    if (body["variable"].contains("declaration")) {
      const auto &decl = body["variable"]["declaration"];
      auto checked = runtime::keys(decl, {"path", "line", "column"});
      if (!checked || !decl.contains("path") || !decl.contains("line"))
        return std::unexpected(invalid("declaration requires path and line"));
      auto path = runtime::text(decl["path"], "declaration.path");
      if (!path) return std::unexpected(invalid(path.error().message));
      for (const auto *field : {"line", "column"}) if (decl.contains(field) &&
          (!decl[field].is_number_integer() || decl[field].get<std::int64_t>() < 1 ||
           decl[field].get<std::int64_t>() > 2147483647))
        return std::unexpected(invalid("declaration line and column must be positive integers"));
    }
  }
  runtime::Request request;
  request.operation = "v2." + operation;
  request.options = body;
  return request;
}
}
