#pragma once

#include "cli/Options.h"

namespace CLI {
class App;
}

namespace facts::cli {

void configureCallGraphOptions(CLI::App &command, CallGraphOptions &options);

} // namespace facts::cli
