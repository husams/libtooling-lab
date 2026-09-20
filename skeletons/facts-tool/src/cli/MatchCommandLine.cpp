#include "cli/MatchCommandLine.h"

#include "cli/ConfigurationOptions.h"
#include "cli/Verbose.h"

#include <CLI/CLI.hpp>

namespace facts::cli {

CLI::App *configureMatch(CLI::App &app, MatchOptions &options) {
  auto *command = app.add_subcommand(
      "match", "Run a dynamic AST matcher and persist supported bound facts");
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
                   "Clang dynamic matcher expression; bindings are optional "
                   "and may use arbitrary names")
      ->required()
      ->type_name("EXPR");
  command
      ->add_option("--traversal", options.traversal,
                   "AST traversal mode: AsIs or "
                   "IgnoreUnlessSpelledInSource (default: AsIs)")
      ->check(CLI::IsMember({"AsIs", "IgnoreUnlessSpelledInSource"}))
      ->type_name("MODE");
  command->add_flag(
      "--capture-source", options.captureSource,
      "Persist the exact source region for each matched function, method, or record definition");
  command->add_option("--format", options.format,
                      "Result output: text with locations or structured JSON")
      ->check(CLI::IsMember({"text", "json"}));
  auto *relationKind = command
      ->add_option("--relation-kind", options.relationKind,
                   "Persist a relation using the selected role bindings")
      ->type_name("KIND");
  const auto bindingOption = [&](const char *flag, std::string &value,
                                 const char *description) {
    command->add_option_function<std::string>(flag,
        [&value, flag](const std::string &name) {
          if (name.empty())
            throw CLI::ValidationError(std::string{flag} + " must not be empty");
          value = name;
        }, description)
        ->trigger_on_parse()
        ->needs(relationKind)
        ->type_name("NAME");
  };
  bindingOption("--source-binding", options.sourceBinding,
                "Relation source binding (default: source)");
  bindingOption("--target-binding", options.targetBinding,
                "Relation target binding (default: target)");
  bindingOption("--site-binding", options.siteBinding,
                "Relation occurrence binding (default: site)");
  bindingOption("--call-binding", options.callBinding,
                "Calls expression binding (default: call)");
  bindingOption("--callee-binding", options.calleeBinding,
                "Calls destination binding (default: callee)");
  command->add_option("sources", options.sources,
                      "Translation units relative to the invocation directory; defaults to imported order");
  return command;
}

} // namespace facts::cli
