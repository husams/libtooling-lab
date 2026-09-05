#include "cli/Dispatch.h"
#include "cli/Verbose.h"
#include "commands/Dependency.h"
#include "commands/Configuration.h"
#include "commands/Extract.h"
#include "commands/Import.h"
#include "commands/Match.h"
#include "commands/analyse/CallGraphCommand.h"
#include "commands/catalog/Commands.h"
#include <format>
#include <iostream>
#include <utility>

namespace facts::cli {
namespace {
std::string_view commandName(const ExtractOptions &) { return "extract"; }
std::string_view commandName(const ImportOptions &) { return "import"; }
std::string_view commandName(const DependencyOptions &) { return "dependency"; }
std::string_view commandName(const CallGraphOptions &) { return "call-graph"; }
std::string_view commandName(const MatchOptions &) { return "match"; }
std::string_view commandName(const ConfigOptions &) { return "config"; }
std::string_view commandName(const RepositoryOptions &) { return "repo"; }
std::string_view commandName(const ComponentOptions &) { return "component"; }
std::string_view commandName(const DirectoryOptions &) { return "dir"; }
std::string_view commandName(const FileOptions &) { return "file"; }
std::string_view commandName(const SymbolOptions &) { return "symbol"; }
template <typename Options>
std::string commandDetails(const Options &options) {
  return std::format("configuration='{}'", options.configuration);
}
std::string commandDetails(const ExtractOptions &options) {
  return std::format("configuration='{}', output='{}', requested_sources={}",
                     options.configuration, options.output,
                     options.sources.size());
}

std::string commandDetails(const ImportOptions &options) {
  return std::format(
      "configuration='{}', compilation_database='{}', requested_sources={}, "
      "components={}",
      options.configuration,
      options.compilationDatabase.empty() ? "fixed commands"
                                          : options.compilationDatabase,
      options.sources.size(), options.components.size());
}

std::string commandDetails(const DependencyOptions &options) {
  return std::format("configuration='{}', output='{}', roots={}",
                     options.configuration, options.output,
                     options.sources.size());
}

std::string commandDetails(const CallGraphOptions &options) {
  return std::format("facts='{}', scope='{}'", options.facts,
                     options.all ? "all" : *options.function);
}

std::string commandDetails(const MatchOptions &options) {
  return std::format("facts='{}', requested_sources={}", options.facts,
                     options.sources.size());
}
std::string commandDetails(const ConfigOptions &) { return {}; }

std::string commandDetails(const SymbolOptions &options) {
  return std::format("facts='{}'", options.facts);
}

std::expected<int, std::string> execute(const ExtractOptions &options) {
  return commands::runExtract(options);
}

std::expected<int, std::string> execute(const ImportOptions &options) {
  return commands::runImport(options);
}

std::expected<int, std::string> execute(const DependencyOptions &options) {
  return commands::runDependency(options);
}

std::expected<int, std::string> execute(const CallGraphOptions &options) {
  return commands::runCallGraph(options);
}

std::expected<int, std::string> execute(const MatchOptions &options) {
  return commands::runMatch(options);
}
std::expected<int, std::string> execute(const ConfigOptions &options) {
  return commands::runConfiguration(options);
}

std::expected<int, std::string> execute(const RepositoryOptions &options) {
  return commands::runRepository(options);
}

std::expected<int, std::string> execute(const ComponentOptions &options) {
  return commands::runComponent(options);
}

std::expected<int, std::string> execute(const DirectoryOptions &options) {
  return commands::runDirectory(options);
}

std::expected<int, std::string> execute(const FileOptions &options) {
  return commands::runFile(options);
}

std::expected<int, std::string> execute(const SymbolOptions &options) {
  return commands::runSymbol(options);
}

int report(std::expected<int, std::string> result) {
  if (!result) {
    const auto prefixed = result.error().starts_with("facts-tool:");
    std::cerr << (prefixed ? "" : "facts-tool: ") << result.error() << '\n';
    if (result.error().starts_with("facts-tool: configuration error:")) return 3;
    if (result.error().starts_with("facts-tool: usage error:")) return 2;
    return 1;
  }
  return *result;
}

} // namespace

int dispatch(Command command) {
  return std::visit(
      [](auto options) {
        const auto name = commandName(options);
        logVerbose(options.verbosity, 1, "facts-tool: {}: starting", name);
        logVerbose(options.verbosity, 2, "facts-tool: {}: {}", name,
                   commandDetails(options));
        auto result = execute(options);
        logVerbose(options.verbosity, 1, "facts-tool: {}: {}", name,
                   result ? "complete" : "failed");
        return report(std::move(result));
      },
      std::move(command));
}

} // namespace facts::cli
