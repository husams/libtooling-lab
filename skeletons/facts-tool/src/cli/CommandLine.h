#pragma once
#include <string>
#include <vector>

namespace facts::cli {

int run(int argc, char **argv);
std::vector<std::string> commandPaths();

} // namespace facts::cli
