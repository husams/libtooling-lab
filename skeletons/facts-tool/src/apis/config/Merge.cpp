#include "apis/config/Options.h"
#include <algorithm>
#include <cstdlib>

namespace facts::apis {
namespace {
void replaceDefault(std::vector<std::string> &defaults, const std::string &name,
                    const std::string &value) {
  for (auto it = defaults.begin(); it != defaults.end();) {
    if (*it == name && std::next(it) != defaults.end())
      it = defaults.erase(it, std::next(it, 2));
    else
      ++it;
  }
  defaults.insert(defaults.end(), {name, value});
}
}
Settings mergeSettings(const CLI::App &app, const Arguments &arguments,
                       Settings saved) {
  const auto &requested = arguments.settings;
  if (app.count("--host")) saved.host = requested.host;
  if (app.count("--port")) saved.port = requested.port;
  if (app.count("--debounce-ms")) saved.debounceMs = requested.debounceMs;
  if (app.count("--timeout")) saved.timeoutSeconds = requested.timeoutSeconds;
  if (app.count("--import-arg")) saved.importArguments = requested.importArguments;
  if (app.count("--extract-arg")) saved.extractArguments = requested.extractArguments;
  if (app.count("--working-directory"))
    saved.workingDirectory = std::filesystem::absolute(arguments.workingDirectory);
  if (app.count("--watch") || arguments.clearWatches) {
    saved.directories.clear();
    for (const auto &path : arguments.watches)
      saved.directories.push_back(std::filesystem::absolute(path));
  }
  if (app.count("--config"))
    replaceDefault(saved.defaults, "--config",
                   std::filesystem::absolute(arguments.configuration).string());
  if (app.count("--conf"))
    replaceDefault(saved.defaults, "--conf",
                   std::filesystem::absolute(arguments.project).string());
  if (const char *token = std::getenv("FACTS_TOOL_API_TOKEN")) saved.token = token;
  if (app.count("--token")) saved.token = requested.token;
  saved.daemon = requested.daemon;
  saved.explicitPort = app.count("--port") != 0;
  return saved;
}
}
