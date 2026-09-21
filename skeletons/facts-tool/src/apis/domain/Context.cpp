#include "apis/domain/Selection.h"
#include <cstdlib>

namespace facts::apis::domain {
Result<Context> resolveContext(const Settings &settings) {
  config::Request request;
  request.workingDirectory = settings.workingDirectory;
  for (std::size_t i = 0; i + 1 < settings.defaults.size(); i += 2) {
    if (settings.defaults[i] == "--conf") request.direct = settings.defaults[i + 1];
    if (settings.defaults[i] == "--config") request.selector = settings.defaults[i + 1];
  }
  if (request.selector.empty())
    if (const auto *value = std::getenv("FACTS_TOOL_CONFIG")) request.selector = value;
  if (!request.selector.empty() && std::filesystem::path(request.selector).is_relative())
    request.selector = (settings.workingDirectory / request.selector).lexically_normal().string();
  try {
    return config::resolve(request)
        .transform([&](config::Resolved configuration) {
          return Context{std::move(configuration), request.selector};
        })
        .transform_error([](const std::string &message) {
          return Error{500, "server_configuration", message};
        });
  } catch (const std::exception &error) {
    return std::unexpected(Error{500, "server_configuration", error.what()});
  }
}
Result<Context> workspaceContext(const Context &context, const std::filesystem::path &root) {
  if (root.empty()) return context;
  config::Request request;
  request.direct = context.configuration.database.string();
  request.selector = context.configurationFile;
  request.workingDirectory = root;
  request.workspaceRoot = true;
  try {
    return config::resolve(request)
        .transform([&](config::Resolved configuration) {
          // A registered clone is the workspace even without a .git directory.
          configuration.projectRoot = root;
          return Context{std::move(configuration), context.configurationFile};
        })
        .transform_error([&](const std::string &message) {
          return Error{500, "workspace_configuration", message,
              {{"project_root", root.string()}, {"stage", "resolve workspace configuration"},
               {"expected", "An accessible active clone and valid project configuration"},
               {"action", "Correct the clone path or its .facts-tool.yaml and retry the job"}}};
        });
  } catch (const std::exception &error) {
    return std::unexpected(Error{500, "workspace_configuration", error.what(),
        {{"project_root", root.string()}, {"stage", "resolve workspace configuration"},
         {"action", "Restore the active clone directory and retry the job"}}});
  }
}
} // namespace facts::apis::domain
