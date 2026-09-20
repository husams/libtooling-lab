#include "apis/config/Parse.h"
#include "apis/config/Options.h"
#include "apis/config/Paths.h"
#include "apis/config/Persistence.h"
#include <iostream>

namespace facts::apis {
std::expected<Settings, int> parseSettings(int argc, char **argv) {
  CLI::App app("Run the asynchronous facts-tool REST server", "facts-tool serve");
  Arguments arguments;
  configureOptions(app, arguments);
  try {
    app.parse(argc - 1, argv + 1);
    if (arguments.serverConfig.empty())
      throw std::runtime_error("--server-config must not be empty");
    if ((app.count("--config") && arguments.configuration.empty()) ||
        (app.count("--conf") && arguments.project.empty()) ||
        (app.count("--log-file") && arguments.logFile.empty()) ||
        (app.count("--working-directory") && arguments.workingDirectory.empty()))
      throw std::runtime_error("configuration paths must not be empty");
    auto settings = loadSettings(std::filesystem::absolute(arguments.serverConfig))
        .transform([&](Settings saved) {
          return mergeSettings(app, arguments, std::move(saved));
        })
        .and_then([&](Settings saved) {
          return normalizeSettings(std::move(saved), argv[0]);
        });
    if (settings) return std::move(*settings);
    std::cerr << "facts-tool: server configuration error: " << settings.error() << '\n';
  } catch (const CLI::ParseError &error) {
    const auto code = app.exit(error);
    return std::unexpected(code == 0 ? 0 : 2);
  } catch (const std::exception &error) {
    std::cerr << "facts-tool: server configuration error: " << error.what() << '\n';
  }
  return std::unexpected(2);
}
}
