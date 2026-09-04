#include "config/Configuration.h"
#include "config/ConfigurationDiscovery.h"

namespace facts::config {
namespace {
std::string settingFor(std::string_view error) {
  for (const auto setting : {"conf_root", "conf_template", "extra_args"})
    if (error.starts_with(setting)) return setting;
  return "configuration";
}

std::string reasonFor(std::string_view setting, std::string_view error) {
  const auto prefix = std::string(setting) + " ";
  return error.starts_with(prefix) ? std::string(error.substr(prefix.size()))
                                  : std::string(error);
}

std::vector<std::string> implicitCandidates(const std::filesystem::path &root) {
  std::vector<std::string> candidates{(root / ".facts-tool.yaml").string()};
  const auto xdg = detail::env("XDG_CONFIG_HOME");
  if (!xdg.empty()) {
    candidates.push_back(
        (std::filesystem::path(xdg) / "facts-tool/config.yaml").string());
  } else if (!detail::env("HOME").empty()) {
    candidates.push_back((std::filesystem::path(detail::env("HOME")) /
                          ".config/facts-tool/config.yaml")
                             .string());
  } else {
    candidates.push_back("${HOME}/.config/facts-tool/config.yaml");
  }
  return candidates;
}
}

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
  const auto direct = !request.direct.empty()
                          ? request.direct
                          : detail::env("FACTS_TOOL_CONF");
  if (!request.loadDefaults && !direct.empty()) {
    result.database = detail::cwd() / direct;
    result.source = request.direct.empty() ? "FACTS_TOOL_CONF" : "--conf";
    result.generated = false;
    return result;
  }
  const auto explicitFile = !request.selector.empty() ? request.selector : detail::env("FACTS_TOOL_CONFIG");
  std::filesystem::path selected;
  const auto candidates = implicitCandidates(root);
  if (!explicitFile.empty()) {
    selected = detail::cwd() / explicitFile;
    if (!std::filesystem::exists(selected)) {
      result.discovery.push_back(selected.string() + " [absent]");
      for (const auto &candidate : candidates)
        result.discovery.push_back(candidate + " [skipped]");
      return std::unexpected("configuration file not found: " +
                             selected.string() + "; searched: " +
                             detail::searched(result.discovery));
    }
    result.discovery.push_back(selected.string() + " [selected]");
    for (const auto &candidate : candidates)
      result.discovery.push_back(candidate + " [skipped]");
    result.source = explicitFile == request.selector ? "--config" : "FACTS_TOOL_CONFIG";
  } else {
    const auto xdg = detail::env("XDG_CONFIG_HOME");
    if (!xdg.empty() && !std::filesystem::path(xdg).is_absolute())
      return std::unexpected("XDG_CONFIG_HOME must be absolute");
    bool found = false;
    for (const auto &candidateText : candidates) {
      const auto candidate = std::filesystem::path(candidateText);
      if (found) {
        result.discovery.push_back(candidate.string() + " [skipped]");
      } else if (candidateText.starts_with("${HOME}")) {
        result.discovery.push_back(candidateText + " [inaccessible: HOME is unset]");
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
      return std::unexpected(settingFor(parsed.error()) + " in " +
                             selected.string() + ": " +
                             reasonFor(settingFor(parsed.error()), parsed.error()) +
                             "; searched: " + detail::searched(result.discovery) +
                             "; set --config or FACTS_TOOL_CONFIG");
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
