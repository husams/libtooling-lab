#include "cli/ConfigurationOptions.h"
#include "cli/catalog/Configure.h"
#include "cli/catalog/Options.h"
#include <CLI/CLI.hpp>

#include <limits>

namespace facts::cli {
namespace {

void symbolOptions(CLI::App &command, SymbolOptions &options) {
  command
      .add_option_function<std::string>(
          "-f,--facts",
          [&options](const std::string &value) {
            if (value.empty())
              throw CLI::ValidationError("--facts must not be empty");
            options.facts = value;
            options.factsProvided = true;
          },
          "Extracted facts database; defaults to facts_template when omitted "
          "(project-scoped templates only)")
      ->trigger_on_parse()
      ->type_name("FILE");
  configurationOptions(command, options.configuration,
                       options.configurationFile);
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
  auto &find = symbolLeaf(*group, "find", "Find matched symbol candidates",
                          options, SymbolOptions::Action::find);
  auto &selector = *find.add_option_group("selector", "Select exactly one");
  selector.require_option(1, 1);
  selector.add_option("--usr", options.usr, "Exact USR");
  selector
      .add_option_function<std::string>(
          "--name",
          [&options](const std::string &value) {
            if (value.empty())
              throw CLI::ValidationError("--name must not be empty");
            options.name = value;
          },
          "Literal qualified-name substring")
      ->trigger_on_parse();
  find.add_option("--kind", options.kind, "Raw Clang index symbol kind");
  find.add_option("--format", options.format, "Output: text or json")
      ->check(CLI::IsMember({"text", "json"}));
  auto *index = group->add_subcommand("index", "Manage matched-symbol index");
  index->require_subcommand(1, 1);
  symbolOptions(*index, options);
  symbolLeaf(*index, "clear", "Clear candidates for one file", options,
             SymbolOptions::Action::clearIndex)
      .add_option("--file-id", options.fileId)
      ->required()
      ->check(CLI::Range(std::int64_t{1},
                         std::numeric_limits<std::int64_t>::max()));
  return group;
}

} // namespace facts::cli
