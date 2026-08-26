#pragma once

#include "cli/catalog/Options.h"
#include <CLI/CLI.hpp>

namespace facts::cli {

template <typename Options>
CLI::App &catalogLeaf(CLI::App &group, const char *name,
                      const char *description, Options &options,
                      typename Options::Action action) {
  auto &leaf = *group.add_subcommand(name, description);
  leaf.add_option("--conf", options.configuration,
                  "Project configuration database")
      ->required()
      ->type_name("FILE");
  leaf.add_option("-v,--verbose", options.verbosity, "Verbosity level")
      ->expected(0, 1)
      ->default_str("1")
      ->check(CLI::Range(0, 3));
  leaf.callback([&options, action] { options.action = action; });
  return leaf;
}

inline void catalogSelector(CLI::App &leaf, CatalogSelector &selector,
                            bool allowName) {
  auto &group = *leaf.add_option_group("selector", "Select exactly one object");
  group.require_option(1, 1);
  group.add_option("--id", selector.id)->check(CLI::PositiveNumber);
  group.add_option("--path", selector.path);
  if (allowName) {
    group.add_option("--name", selector.name);
  }
}

} // namespace facts::cli
