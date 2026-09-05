#include "cli/catalog/Configure.h"
#include "cli/ConfigurationOptions.h"
#include "cli/catalog/Options.h"
#include <CLI/CLI.hpp>

namespace facts::cli {
namespace {

void symbolOptions(CLI::App &command, SymbolOptions &options) {
  command
      .add_option("-f,--facts", options.facts,
                  "Extracted facts database; defaults to facts_template "
                  "when omitted (project-scoped templates only)")
      ->type_name("FILE");
  configurationOptions(command, options.configuration, options.configurationFile);
  command.add_option("-v,--verbose", options.verbosity, "Verbosity level")
      ->expected(0, 1)
      ->default_str("1")
      ->check(CLI::Range(0, 3));
}

CLI::App &symbolLeaf(CLI::App &group, const char *name, const char *description,
                     SymbolOptions &options, SymbolOptions::Action action) {
  auto &leaf = *group.add_subcommand(name, description);
  symbolOptions(leaf, options);
  leaf.callback([&options, action] { options.action = action; });
  return leaf;
}

} // namespace

CLI::App *configureSymbol(CLI::App &app, SymbolOptions &options) {
  auto *group = app.add_subcommand("symbol", "Inspect extracted symbols");
  group->require_subcommand(1, 1);
  symbolOptions(*group, options);
  symbolLeaf(*group, "list", "List extracted symbols", options,
             SymbolOptions::Action::list)
      .alias("ls");
  symbolLeaf(*group, "show", "Show symbols by exact qualified name", options,
             SymbolOptions::Action::show)
      .add_option("qualified-name", options.qualifiedName)
      ->required();
  symbolLeaf(*group, "browser", "Browse extracted symbols interactively",
             options, SymbolOptions::Action::browser);
  return group;
}

} // namespace facts::cli
