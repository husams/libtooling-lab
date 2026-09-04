#include "config/Configuration.h"

#include <cstdlib>

namespace facts::config {
namespace {
std::string env(const char *name) {
  const auto *value = std::getenv(name);
  return value ? value : "";
}
bool present(const char *name) { return std::getenv(name) != nullptr; }
std::filesystem::path cwd() { return std::filesystem::canonical("."); }
std::filesystem::path projectRoot(std::filesystem::path path) {
  for (;;) {
    if (std::filesystem::exists(path / ".facts-tool.yaml") ||
        std::filesystem::exists(path / ".git")) return path;
    const auto parent = path.parent_path();
    if (parent == path) return path;
    path = parent;
  }
}
std::filesystem::path builtInRoot() {
  const auto data = env("XDG_DATA_HOME");
  return (!data.empty() ? std::filesystem::path(data)
                        : std::filesystem::path(env("HOME")) / ".local/share") /
         "facts-tool";
}
}

std::expected<Resolved, std::string> resolve(const Request &request) {
  const auto root = projectRoot(cwd());
  Resolved result{.projectRoot = root, .source = "built-in", .storageRoot = env("XDG_DATA_HOME"), .templateText = "{relative_path}/{filename}.db"};
  if (request.selector.empty() && present("FACTS_TOOL_CONFIG") && env("FACTS_TOOL_CONFIG").empty())
    return std::unexpected("FACTS_TOOL_CONFIG must not be empty");
  if (request.direct.empty() && present("FACTS_TOOL_CONF") && env("FACTS_TOOL_CONF").empty())
    return std::unexpected("FACTS_TOOL_CONF must not be empty");
  const auto explicitFile = !request.selector.empty() ? request.selector : env("FACTS_TOOL_CONFIG");
  std::filesystem::path selected;
  if (!explicitFile.empty()) {
    selected = cwd() / explicitFile;
    if (!std::filesystem::exists(selected))
      return std::unexpected("configuration file not found: " + selected.string());
    result.source = explicitFile == request.selector ? "--config" : "FACTS_TOOL_CONFIG";
  } else {
    const auto xdg = env("XDG_CONFIG_HOME");
    if (!xdg.empty() && !std::filesystem::path(xdg).is_absolute())
      return std::unexpected("XDG_CONFIG_HOME must be absolute");
    const std::vector<std::filesystem::path> candidates = {
        root / ".facts-tool.yaml",
        xdg.empty() ? std::filesystem::path{} : std::filesystem::path(xdg) / "facts-tool/config.yaml",
        env("HOME").empty() ? std::filesystem::path{} : std::filesystem::path(env("HOME")) / ".config/facts-tool/config.yaml"};
    for (const auto &candidate : candidates)
      if (!candidate.empty() && std::filesystem::exists(candidate)) { selected = candidate; break; }
  }
  if (!selected.empty()) {
    auto parsed = readYaml(selected, result);
    if (!parsed) return std::unexpected(parsed.error());
    result = std::move(*parsed);
    result.source = selected.string();
    if (result.storageRoot.empty()) result.storageRoot = builtInRoot();
    if (result.templateText.empty()) result.templateText = "{relative_path}/{filename}.db";
  }
  if (selected.empty()) {
    const auto data = env("XDG_DATA_HOME");
    if (!data.empty() && !std::filesystem::path(data).is_absolute())
      return std::unexpected("XDG_DATA_HOME must be absolute");
    if (data.empty() && env("HOME").empty())
      return std::unexpected("unresolved conf; set --conf or FACTS_TOOL_CONF");
    const auto base = !data.empty() ? std::filesystem::path(data) : std::filesystem::path(env("HOME")) / ".local/share";
    if (base.empty()) return std::unexpected("unresolved conf; set --conf or FACTS_TOOL_CONF");
    result.storageRoot = base / "facts-tool";
  }
  if (!request.direct.empty()) result.database = cwd() / request.direct, result.source = "--conf", result.generated = false;
  else if (!env("FACTS_TOOL_CONF").empty()) result.database = cwd() / env("FACTS_TOOL_CONF"), result.source = "FACTS_TOOL_CONF", result.generated = false;
  if (result.database.empty()) {
    auto rendered = renderDatabasePath(result);
    if (!rendered) return std::unexpected("facts-tool: configuration error: " + rendered.error());
    result.database = *rendered;
  }
  return result;
}
} // namespace facts::config
