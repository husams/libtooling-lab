#include "cli/MatchCommandLine.h"

#include "cli/Verbose.h"

#include <CLI/CLI.hpp>

namespace facts::cli {

CLI::App *configureMatch(CLI::App &app, MatchOptions &options) {
  auto *command = app.add_subcommand(
      "match", "Run a dynamic AST matcher and persist bound facts");
  command
      ->add_option("-v,--verbose", options.verbosity,
                   "Verbosity level; defaults to 1 when omitted")
      ->expected(0, 1)
      ->default_str("1")
      ->check(CLI::Range(0, maximumVerbosity));
  command
      ->add_option("-f,--facts", options.facts,
                   "Imported SQLite project and facts database")
      ->required()
      ->type_name("FILE");
  command
      ->add_option("--matcher", options.matcher,
                   "Clang dynamic matcher expression")
      ->required()
      ->type_name("EXPR");
  command
      ->add_option("--relation-kind", options.relationKind,
                   "Relation kind for source/target bindings")
      ->type_name("KIND");
  command->add_option("sources", options.sources,
                      "Translation units; defaults to imported order");
  return command;
}

} // namespace facts::cli
