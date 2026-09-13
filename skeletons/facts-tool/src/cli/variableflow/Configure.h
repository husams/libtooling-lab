#pragma once

#include "cli/Options.h"

namespace CLI {
class App;
}

namespace facts::cli {
void configureVariableFlow(CLI::App &command, VariableFlowOptions &options);
} // namespace facts::cli
