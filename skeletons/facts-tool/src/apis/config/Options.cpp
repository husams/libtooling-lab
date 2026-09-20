#include "apis/config/Options.h"

namespace facts::apis {
void configureOptions(CLI::App &app, Arguments &arguments) {
  auto &settings = arguments.settings;
  app.add_option("--host", settings.host, "Listener address; defaults to loopback");
  app.add_option("--port", settings.port, "TCP port; 0 allocates an available port")
      ->check(CLI::Range(0, 65535));
  app.add_flag("--daemon", settings.daemon, "Run in the background after readiness");
  app.add_option("--log-file", arguments.logFile,
                 "Server log file; foreground defaults to stderr");
  auto *level = app.add_option("--log-level", arguments.logLevel,
      "Server log level: off, error, warning, info, debug, trace")
      ->check(CLI::IsMember({"off", "error", "warning", "info", "debug", "trace"}));
  auto *verbosity = app.add_option("-v,--verbose", arguments.verbosity,
      "Server verbosity: 0 errors, 1 info, 2 debug, 3 trace")
      ->check(CLI::Range(0, 3));
  level->excludes(verbosity);
  app.add_option("--server-config", arguments.serverConfig,
                 "Server settings file, created or updated after binding");
  app.add_option("--config", arguments.configuration, "Default CLI YAML configuration");
  app.add_option("-c,--conf", arguments.project, "Default CLI project database");
  app.add_option("--working-directory", arguments.workingDirectory,
                 "Working directory for CLI requests");
  app.add_flag("--watch", arguments.enableWatch,
               "Monitor active clones of repositories in the project database");
  app.add_flag("--no-watch", arguments.disableWatch, "Disable filesystem monitoring")
      ->excludes("--watch");
  app.add_option("--token", settings.token,
                 "Bearer token; prefer FACTS_TOOL_API_TOKEN to avoid shell history");
  app.add_option("--debounce-ms", settings.debounceMs, "Filesystem event debounce")
      ->check(CLI::Range(1, 3600000));
  app.add_option("--timeout", settings.timeoutSeconds, "CLI job timeout in seconds")
      ->check(CLI::Range(1, 86400));
  app.add_option("--import-arg", settings.importArguments,
                 "Automatic import argument; repeat --import-arg=VALUE")
      ->expected(1)->take_all();
  app.add_option("--extract-arg", settings.extractArguments,
                 "Automatic extraction argument; repeat --extract-arg=VALUE")
      ->expected(1)->take_all();
}
}
