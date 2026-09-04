#include "config/Configuration.h"
#include "config/ConfigurationDiscovery.h"

namespace facts::config {
std::expected<Resolved, std::string> resolve(const Request &request) {
  const auto root = detail::projectRoot(detail::cwd());
  Resolved result{.projectRoot = root,
                  .templateText = "{relative_path}/{filename}.db",
                  .source = "built-in",
                  .storageRootSource = "built-in",
                  .templateSource = "built-in",
                  .extraArgumentsSource = "built-in"};
  if (request.selector.empty() && detail::present("FACTS_TOOL_CONFIG") && detail::env("FACTS_TOOL_CONFIG").empty())
    return std::unexpected("FACTS_TOOL_CONFIG must not be empty");
  if (request.direct.empty() && detail::present("FACTS_TOOL_CONF") && detail::env("FACTS_TOOL_CONF").empty())
    return std::unexpected("FACTS_TOOL_CONF must not be empty");
  const auto explicitFile = !request.selector.empty() ? request.selector : detail::env("FACTS_TOOL_CONFIG");
  std::filesystem::path selected;
  if (!explicitFile.empty()) {
    selected = detail::cwd() / explicitFile;
    if (!std::filesystem::exists(selected)) {
      result.discovery.push_back(selected.string() + " [absent]");
      return std::unexpected("configuration file not found: " +
                             selected.string() + "; searched: " +
                             detail::searched(result.discovery));
    }
    result.discovery.push_back(selected.string() + " [selected]");
    result.source = explicitFile == request.selector ? "--config" : "FACTS_TOOL_CONFIG";
  } else {
    const auto xdg = detail::env("XDG_CONFIG_HOME");
    if (!xdg.empty() && !std::filesystem::path(xdg).is_absolute())
      return std::unexpected("XDG_CONFIG_HOME must be absolute");
    const std::vector<std::filesystem::path> candidates = {
        root / ".facts-tool.yaml",
        xdg.empty() ? std::filesystem::path{} : std::filesystem::path(xdg) / "facts-tool/config.yaml",
        detail::env("HOME").empty() ? std::filesystem::path{} : std::filesystem::path(detail::env("HOME")) / ".config/facts-tool/config.yaml"};
    bool found = false;
    for (const auto &candidate : candidates) {
      if (candidate.empty()) continue;
      if (found) {
        result.discovery.push_back(candidate.string() + " [skipped]");
      } else if (std::filesystem::exists(candidate)) {
        selected = candidate;
        found = true;
        result.discovery.push_back(candidate.string() + " [selected]");
      } else {
        result.discovery.push_back(candidate.string() + " [absent]");
      }
    }
  }
  if (!selected.empty()) {
    auto parsed = readYaml(selected, result);
    if (!parsed)
      return std::unexpected(parsed.error() + " in " + selected.string() +
                             "; searched: " + detail::searched(result.discovery));
    result = std::move(*parsed);
    result.source = selected.string();
    if (result.storageRoot.empty()) {
      result.storageRoot = detail::builtInRoot();
      result.storageRootSource = "built-in";
    }
    if (result.templateText.empty()) {
      result.templateText = "{relative_path}/{filename}.db";
      result.templateSource = "built-in";
    }
  }
  if (selected.empty() && request.direct.empty() && !detail::present("FACTS_TOOL_CONF")) {
    const auto data = detail::env("XDG_DATA_HOME");
    if (!data.empty() && !std::filesystem::path(data).is_absolute())
      return std::unexpected("XDG_DATA_HOME must be absolute");
    if (data.empty() && detail::env("HOME").empty())
      return std::unexpected("unresolved conf; searched: " +
                             detail::searched(result.discovery) +
                             "; set --conf or FACTS_TOOL_CONF");
    const auto base = !data.empty() ? std::filesystem::path(data) : std::filesystem::path(detail::env("HOME")) / ".local/share";
    if (base.empty())
      return std::unexpected("unresolved conf; searched: " +
                             detail::searched(result.discovery) +
                             "; set --conf or FACTS_TOOL_CONF");
    result.storageRoot = base / "facts-tool";
    result.storageRootSource = "built-in";
  }
  if (!request.direct.empty()) result.database = detail::cwd() / request.direct, result.source = "--conf", result.generated = false;
  else if (!detail::env("FACTS_TOOL_CONF").empty()) result.database = detail::cwd() / detail::env("FACTS_TOOL_CONF"), result.source = "FACTS_TOOL_CONF", result.generated = false;
  if (result.database.empty()) {
    auto rendered = renderDatabasePath(result);
    if (!rendered) return std::unexpected(rendered.error());
    result.database = *rendered;
  }
  return result;
}
} // namespace facts::config
