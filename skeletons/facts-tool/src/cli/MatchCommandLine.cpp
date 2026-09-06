#include "cli/MatchCommandLine.h"

#include "cli/ConfigurationOptions.h"
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
  configurationOptions(*command, options.configuration,
                       options.configurationFile);
  command->add_option_function<std::string>(
      "-f,--facts",
      [&options](const std::string &value) {
        if (value.empty())
          throw CLI::ValidationError("--facts must not be empty");
        options.facts = value;
        options.factsProvided = true;
      },
      "SQLite facts database; defaults to facts_template when omitted")
      ->trigger_on_parse()
      ->type_name("FILE");
  command
      ->add_option("--matcher", options.matcher,
                   "Clang dynamic matcher expression; bind symbol, "
                   "call+callee, or source+target[+site]")
      ->required()
      ->type_name("EXPR");
  command
      ->add_option("--relation-kind", options.relationKind,
                   "Relation kind for source/target bindings; required for "
                   "relation contracts")
      ->type_name("KIND");
  command->add_option("sources", options.sources,
                      "Translation units relative to the invocation directory; defaults to imported order");
  return command;
}

} // namespace facts::cli
