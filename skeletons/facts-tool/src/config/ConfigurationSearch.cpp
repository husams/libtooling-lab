#include "config/Configuration.h"
#include "config/ConfigurationDiscovery.h"
#include "config/ConfigurationMerge.h"

namespace facts::config {
namespace {

enum class Presence { Optional, Required };

// Checks one candidate file, records it in the discovery trail, and parses
// it when present. A stat error is treated the same as "present" so the
// underlying I/O failure surfaces from readTier() itself.
std::expected<std::optional<Tier>, std::string>
readCandidate(const std::filesystem::path &path, bool applyPathSettings,
             Presence presence, Resolved &value) {
  std::error_code error;
  const bool exists = std::filesystem::exists(path, error);
  if (!error && !exists) {
    value.discovery.push_back(path.string() + " [absent]");
    if (presence == Presence::Required)
      return std::unexpected(detail::settingError(
          "configuration", path.string(), "file not found", value.discovery));
    return std::optional<Tier>{};
  }
  value.discovery.push_back(path.string() + (error ? " [inaccessible]" : " [found]"));
  auto tier = readTier(path, applyPathSettings);
  if (!tier) return std::unexpected(detail::keyError(tier.error(), path, value.discovery));
  return std::optional<Tier>{std::move(*tier)};
}

std::expected<Resolved, std::string> generate(Resolved value) {
  if (value.storageRoot.empty()) {
    const auto data = detail::env("XDG_DATA_HOME");
    if (!data.empty() && !std::filesystem::path(data).is_absolute())
      return std::unexpected("XDG_DATA_HOME must be absolute");
    if (data.empty() && detail::env("HOME").empty())
      return std::unexpected("unresolved conf; searched: " +
                             detail::searched(value.discovery) +
                             "; set --conf or FACTS_TOOL_CONF");
    value.storageRoot = detail::builtInRoot();
  }
  return renderDatabasePath(value).transform([&](auto path) {
    value.database = std::move(path);
    return value;
  });
}

} // namespace

std::expected<Resolved, std::string> resolve(const Request &request, Resolved *partial) {
  const auto direct = !request.direct.empty() ? request.direct : detail::env("FACTS_TOOL_CONF");
  if (request.direct.empty() && detail::present("FACTS_TOOL_CONF") && direct.empty())
    return std::unexpected("FACTS_TOOL_CONF must not be empty");
  Resolved value{.projectRoot = detail::cwd(),
                 .templateText = "{relative_path}/{filename}.db",
                 .source = direct.empty() ? "generated" : "direct",
                 .generated = direct.empty(),
                 .storageRootSource = "built-in",
                 .templateSource = "built-in",
                 .factsTemplateSource = "built-in",
                 .extraArgumentsSource = "built-in"};
  if (partial) *partial = value;
  const auto useDirect = [&](Resolved resolved) {
    resolved.database = (detail::cwd() / direct).lexically_normal();
    resolved.source = request.direct.empty() ? "FACTS_TOOL_CONF" : "--conf";
    return resolved;
  };
  if (!request.loadDefaults && !direct.empty()) return useDirect(value);
  if (request.selector.empty() && detail::present("FACTS_TOOL_CONFIG") &&
      detail::env("FACTS_TOOL_CONFIG").empty())
    return std::unexpected("FACTS_TOOL_CONFIG must not be empty");
  value.projectRoot = detail::projectRoot(value.projectRoot);
  const auto selector = !request.selector.empty() ? request.selector
                                                 : detail::env("FACTS_TOOL_CONFIG");
  const auto xdgConfig = detail::env("XDG_CONFIG_HOME");
  if (!xdgConfig.empty() && !std::filesystem::path(xdgConfig).is_absolute())
    return std::unexpected("XDG_CONFIG_HOME must be absolute");

  detail::MergeContext context;
  if (!selector.empty()) {
    auto configFile = readCandidate((detail::cwd() / selector).lexically_normal(),
                                    value.generated, Presence::Required, value);
    if (partial) *partial = value;
    if (!configFile) return std::unexpected(configFile.error());
    context.configFile = std::move(*configFile);
  }
  auto project = readCandidate(detail::projectConfigPath(value.projectRoot), value.generated,
                               Presence::Optional, value);
  if (partial) *partial = value;
  if (!project) return std::unexpected(project.error());
  context.project = std::move(*project);

  const auto userCandidate = detail::userConfigCandidate();
  if (userCandidate.starts_with("${HOME}")) {
    value.discovery.push_back(userCandidate + " [inaccessible: HOME is unset]");
  } else {
    auto user = readCandidate(userCandidate, value.generated, Presence::Optional, value);
    if (partial) *partial = value;
    if (!user) return std::unexpected(user.error());
    context.user = std::move(*user);
  }

  value = detail::mergeTiers(std::move(value), context);
  if (partial) *partial = value;
  return direct.empty() ? generate(std::move(value)) : useDirect(std::move(value));
}

} // namespace facts::config
