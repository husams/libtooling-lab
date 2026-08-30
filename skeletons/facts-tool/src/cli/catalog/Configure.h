#pragma once

#include "cli/catalog/Options.h"

namespace CLI {
class App;
}

namespace facts::cli {
CLI::App *configureRepository(CLI::App &app, RepositoryOptions &options);
CLI::App *configureComponent(CLI::App &app, ComponentOptions &options);
CLI::App *configureDirectory(CLI::App &app, DirectoryOptions &options);
CLI::App *configureFile(CLI::App &app, FileOptions &options);
CLI::App *configureSymbol(CLI::App &app, SymbolOptions &options);
} // namespace facts::cli
