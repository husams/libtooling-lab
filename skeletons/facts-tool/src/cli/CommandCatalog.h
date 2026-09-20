#pragma once
#include <CLI/CLI.hpp>
#include <string>
#include <vector>

namespace facts::cli {
inline void collectCommandPaths(const CLI::App &app, const std::string &prefix,
                                std::vector<std::string> &paths) {
  for (const auto *child : app.get_subcommands([](const CLI::App *value) {
         return !value->get_name().empty() && value->get_name() != "serve";
       })) {
    std::vector<std::string> names{child->get_name()};
    names.insert(names.end(), child->get_aliases().begin(), child->get_aliases().end());
    for (const auto &name : names) {
      const auto path = prefix.empty() ? name : prefix + "/" + name;
      paths.push_back(path);
      collectCommandPaths(*child, path, paths);
    }
  }
}
}
