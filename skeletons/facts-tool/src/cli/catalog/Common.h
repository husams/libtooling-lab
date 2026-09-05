#pragma once

#include "cli/catalog/Options.h"
#include "cli/ConfigurationOptions.h"
#include <CLI/CLI.hpp>

namespace facts::cli {

// Shared options accepted both on a catalog group (`repo`, `component`, `dir`)
// and on each of its leaves, so `-c` works on either side of the subcommand.
template <typename Options>
void catalogOptions(CLI::App &command, Options &options) {
  configurationOptions(command, options.configuration, options.configurationFile);
  command.add_option("-v,--verbose", options.verbosity, "Verbosity level")
      ->expected(0, 1)
      ->default_str("1")
      ->check(CLI::Range(0, 3));
}

template <typename Options>
CLI::App &catalogGroup(CLI::App &app, const char *name, const char *description,
                       Options &options) {
  auto &group = *app.add_subcommand(name, description);
  group.require_subcommand(1, 1);
  catalogOptions(group, options);
  return group;
}

template <typename Options>
CLI::App &catalogLeaf(CLI::App &group, const char *name,
                      const char *description, Options &options,
                      typename Options::Action action) {
  auto &leaf = *group.add_subcommand(name, description);
  catalogOptions(leaf, options);
  leaf.callback([&options, action] {
    options.action = action;
  });
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
