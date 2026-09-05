#pragma once

#include "cli/Options.h"

namespace CLI {
class App;
}

namespace facts::cli {
CLI::App *configureMatch(CLI::App &app, MatchOptions &options);
}
