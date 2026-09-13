#include "commands/variableflow/Command.h"

#include "analysis/variableflow/Engine.h"
#include "commands/CompilationDatabase.h"
#include "commands/ConfigurationSupport.h"
#include "commands/DatabasePaths.h"
#include "commands/ExtractionSetup.h"
#include "storage/variableflow/Database.h"
#include "tooling/StoredCompilationDatabase.h"

#include <filesystem>
#include <iostream>

namespace facts::commands {
namespace {
struct Inputs {
  config::Resolved configuration;
  CompilationDatabasePtr database;
  std::vector<std::string> sources;
  std::filesystem::path factsPath;
  std::filesystem::path outputPath;
};

std::expected<Inputs, std::string>
resolveInputs(const cli::VariableFlowOptions &options) {
  if (options.outputProvided && options.output.empty())
    return std::unexpected(
        "facts-tool: usage error: --output must not be empty");
  auto resolved = loadConfiguration(options.configuration,
                                    options.configurationFile, false, true);
  if (!resolved)
    return std::unexpected(resolved.error());
  const auto requested = normalizeSourceSelectors(options.sources);
  auto database = ::facts::loadStoredCompilationDatabase(
      resolved->database.string(), requested);
  if (!database)
    return std::unexpected("cannot load project configuration: " +
                           database.error());
  auto commands = requireStoredCommands(std::move(*database));
  if (!commands)
    return std::unexpected(commands.error());
  const auto sources = selectSources(**commands, requested);
  std::filesystem::path factsPath;
  if (!resolved->factsTemplate.empty()) {
    auto facts = resolveFactsOutput(*resolved, sources);
    if (facts)
      factsPath = *facts;
    else if (!options.outputProvided)
      return std::unexpected(
          facts.error() +
          "; pass --output for a standalone variable-flow artifact");
  } else if (!options.outputProvided) {
    return std::unexpected(
        "facts-tool: configuration error: no facts_template is configured; "
        "pass --output for a standalone variable-flow artifact");
  }
  const auto output = [&] {
    if (options.outputProvided)
      return std::filesystem::path(options.output);
    auto value = factsPath;
    value.replace_extension(".variable-flow.db");
    return value;
  }();
  for (const auto &peer : {resolved->database, factsPath}) {
    if (peer.empty())
      continue;
    auto distinct = validateDatabasePaths(output.string(), peer.string());
    if (!distinct)
      return std::unexpected("facts-tool: configuration error: " +
                             distinct.error());
  }
  for (const auto &source : sources) {
    auto distinct = validateDatabasePaths(output.string(), source);
    if (!distinct)
      return std::unexpected("facts-tool: configuration error: " +
                             distinct.error());
    if (factsPath.empty() && !resolved->factsTemplate.empty()) {
      auto path = resolveFactsOutput(*resolved, {source});
      if (!path)
        return std::unexpected(path.error());
      auto separate = validateDatabasePaths(output.string(), path->string());
      if (!separate)
        return std::unexpected("facts-tool: configuration error: " +
                               separate.error());
    }
  }
  return Inputs{std::move(*resolved), std::move(*commands), sources, factsPath,
                output};
}

variableflow::Request request(const cli::VariableFlowOptions &options) {
  return variableflow::Request{*options.function, *options.variable,
                               options.line, options.maxDepth};
}
} // namespace

std::expected<int, std::string>
runVariableFlow(const cli::VariableFlowOptions &options) {
  if (!options.function || options.function->empty() || !options.variable ||
      options.variable->empty())
    return std::unexpected("facts-tool: usage error: --function and --variable "
                           "must not be empty");
  return resolveInputs(options)
      .and_then([&](Inputs inputs) {
        return mergedArguments(inputs.configuration.extraArguments,
                               options.extraArguments,
                               options.extraArgumentsProvided)
            .transform([&](auto arguments) {
              return std::pair{std::move(inputs), std::move(arguments)};
            });
      })
      .and_then([&](auto prepared) {
        auto &[inputs, arguments] = prepared;
        auto adjusted =
            appendExtraArguments(std::move(inputs.database), arguments);
        auto graph =
            variableflow::analyse(*adjusted, inputs.sources, request(options));
        if (!graph)
          return std::expected<int, std::string>{
              std::unexpected(graph.error())};
        variableflow::RunMetadata metadata{
            inputs.configuration.database.string(),
            inputs.factsPath.string(),
            *options.function,
            *options.variable,
            inputs.sources,
            options.line,
            options.maxDepth,
            "facts-tool variable-flow v1",
            "path-insensitive, context-insensitive potential dependencies; "
            "unknown aliases and dynamic dispatch are boundaries"};
        return variableflow::persist(inputs.outputPath, metadata, *graph)
            .transform([&](std::int64_t runId) {
              std::cout << "facts-tool: variable flow run " << runId << ' '
                        << graph->status << '\n';
              return 0;
            });
      });
}
} // namespace facts::commands
