#pragma once
#include "apis/config/Settings.h"
#include <CLI/CLI.hpp>

namespace facts::apis {
struct Arguments {
  Settings settings;
  std::string serverConfig = ".facts-tool-server.yaml";
  std::string configuration;
  std::string project;
  std::string workingDirectory;
  std::vector<std::string> watches;
  bool clearWatches = false;
};
void configureOptions(CLI::App &app, Arguments &arguments);
Settings mergeSettings(const CLI::App &app, const Arguments &arguments,
                       Settings saved);
}
