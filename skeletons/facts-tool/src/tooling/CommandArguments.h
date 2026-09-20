#pragma once
#include <algorithm>
#include <string>
#include <vector>

namespace facts {
inline void appendCommandOptions(std::vector<std::string> &arguments,
                                 const std::vector<std::string> &options) {
  arguments.insert(std::ranges::find(arguments, "--"), options.begin(), options.end());
}
}
