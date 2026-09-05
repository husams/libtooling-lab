#include "config/Configuration.h"
#include "config/ConfigurationDiscovery.h"
#include "config/ConfigurationMerge.h"

namespace facts::config {
namespace {

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

  // Every tier is checked before failing, so an invalid file names its own
  // setting/path and "config show" still reports the full discovery trail
  // (partial provenance) rather than stopping at the first problem.
  detail::MergeContext context;
  std::string firstError;
  const auto record = [&](auto result, auto &slot) {
    if (partial) *partial = value;
    if (result) slot = std::move(*result);
    else if (firstError.empty()) firstError = result.error();
  };
  if (!selector.empty())
    record(detail::readCandidate((detail::cwd() / selector).lexically_normal(), value.generated,
                                 detail::Presence::Required, value),
          context.configFile);
  record(detail::readCandidate(detail::projectConfigPath(value.projectRoot), value.generated,
                               detail::Presence::Optional, value),
        context.project);

  const auto userCandidate = detail::userConfigCandidate();
  if (userCandidate.starts_with("${HOME}")) {
    value.discovery.push_back(userCandidate + " [inaccessible: HOME is unset]");
    if (partial) *partial = value;
  } else {
    record(detail::readCandidate(userCandidate, value.generated, detail::Presence::Optional, value),
          context.user);
  }
  if (!firstError.empty()) return std::unexpected(firstError);

  value = detail::mergeTiers(std::move(value), context);
  if (partial) *partial = value;
  return direct.empty() ? generate(std::move(value)) : useDirect(std::move(value));
}

} // namespace facts::config
