#pragma once

#include "cli/catalog/Options.h"

namespace CLI {
class App;
}

namespace facts::cli {
CLI::App *configureRepository(CLI::App &app, RepositoryOptions &options);
CLI::App *configureComponent(CLI::App &app, ComponentOptions &options);
CLI::App *configureDirectory(CLI::App &app, DirectoryOptions &options);
} // namespace facts::cli
