#include "config/ConfigurationSearch.h"

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
  return renderDatabasePath(value)
      .transform([&](auto path) {
        value.database = std::move(path);
        return value;
      })
      .transform_error([&](const auto &reason) {
        if (reason.starts_with("conf_template escapes conf_root:") ||
            value.source == "built-in") return reason;
        return detail::fileError(value, reason);
      });
}
}

std::expected<Resolved, std::string> resolve(const Request &request, Resolved *partial) {
  const auto direct = !request.direct.empty() ? request.direct
                                              : detail::env("FACTS_TOOL_CONF");
  if (request.direct.empty() && detail::present("FACTS_TOOL_CONF") && direct.empty())
    return std::unexpected("FACTS_TOOL_CONF must not be empty");
  Resolved value{.projectRoot = detail::cwd(),
                 .templateText = "{relative_path}/{filename}.db",
                 .source = "built-in",
                 .generated = direct.empty(),
                 .storageRootSource = "built-in",
                 .templateSource = "built-in",
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
  return detail::discover(std::move(value), selector, partial)
      .and_then([&](Resolved resolved) -> std::expected<Resolved, std::string> {
        if (partial) *partial = resolved;
        return direct.empty() ? generate(std::move(resolved))
                              : useDirect(std::move(resolved));
      });
}
} // namespace facts::config
