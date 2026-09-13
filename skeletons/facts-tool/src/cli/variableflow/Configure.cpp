#include "cli/variableflow/Configure.h"

#include "cli/ConfigurationOptions.h"
#include "cli/Verbose.h"

#include <CLI/CLI.hpp>
#include <limits>

namespace facts::cli {
void configureVariableFlow(CLI::App &command, VariableFlowOptions &options) {
  command
      .add_option("-v,--verbose", options.verbosity,
                  "Verbosity level: 0=quiet, 1=stages, 2=details, 3=trace")
      ->default_str("1")
      ->check(CLI::Range(0, maximumVerbosity))
      ->type_name("LEVEL");
  configurationOptions(command, options.configuration,
                       options.configurationFile);
  command
      .add_option("--function", options.function,
                  "Function name, signature, or USR containing the variable")
      ->required()
      ->type_name("NAME_SIGNATURE_OR_USR");
  command
      .add_option("--variable", options.variable,
                  "Variable name or USR to trace")
      ->required()
      ->type_name("NAME_OR_USR");
  command
      .add_option("--line", options.line,
                  "Declaration line when the variable name is ambiguous")
      ->check(CLI::PositiveNumber)
      ->type_name("DECL_LINE");
  command
      .add_option("--max-depth", options.maxDepth,
                  "Maximum interprocedural call depth; 0 is intraprocedural")
      ->check(CLI::Range(0U, std::numeric_limits<unsigned>::max()))
      ->type_name("N");
  auto *output = command.add_option(
      "-o,--output", options.output,
      "Standalone variable-flow SQLite database; defaults beside facts db");
  output->each([&options](std::string) { options.outputProvided = true; });
  output->type_name("FILE");
  command
      .add_option_function<std::string>(
          "--extra-arg",
          [&options](const std::string &argument) {
            options.extraArgumentsProvided = true;
            options.extraArguments.push_back(argument);
          },
          "Compiler argument overriding matching YAML options; repeatable")
      ->trigger_on_parse()
      ->type_name("ARG");
  command.add_option(
      "sources", options.sources,
      "Translation-unit sources; defaults to all imported files");
}
} // namespace facts::cli
