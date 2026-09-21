#include "apis/operations/Compilation.h"
#include "apis/operations/compilation/Candidates.h"
#include "tooling/CompilationCommandCodec.h"
#include "commands/CompilationDatabase.h"
#include "tooling/StoredCompilationDatabase.h"

namespace facts::apis::operations {
domain::Error compilationFailure(const domain::Context &context,
    const domain::ResolvedFile &file, domain::Error error) {
  if (!error.details.is_object()) error.details = nlohmann::json::object();
  error.details["path"] = file.path.string();
  error.details["repository"] = file.repository;
  error.details["project_root"] = file.clone ? file.clone->path :
      context.configuration.projectRoot.string();
  if (file.clone) error.details["clone_path"] = file.clone->path;
  // Report the input command in the selected clone/header context. Do not
  // repeat driver probing or internal Clang adjustments while handling errors.
  try {
    const std::vector<std::string> sources{file.path.string()};
    auto compilation = loadStoredCompilationDatabase(context.configuration.database.string(), sources);
    if (!compilation) return error;
    auto adjusted = commands::appendExtraArguments(std::move(*compilation),
        context.configuration.extraArguments);
    auto commands = nlohmann::json::array();
    for (const auto &command : adjusted->getCompileCommands(file.path.string())) {
      if (command.CommandLine.empty()) continue;
      commands.push_back({{"source_file", command.Filename},
          {"driver", command.CommandLine.front()}, {"working_directory", command.Directory},
          {"arguments", command.CommandLine}});
    }
    error.details["compilation_commands"] = std::move(commands);
  } catch (const std::exception &) {
    // A secondary filesystem/SQLite failure must not replace the original error.
  }
  return error;
}
domain::Result<CompilationContext> prepareCompilation(
    const domain::Context &context, const domain::ResolvedFile &file) {
  const auto project = normalizeCompilationPath(context.configuration.database);
  return compilation::hasCommand(context, file)
      .and_then([&](bool direct) -> domain::Result<CompilationContext> {
        if (direct) return CompilationContext{project, {}};
        return compilation::candidates(context).and_then([&](auto candidates) {
          return compilation::knownIncluders(file).and_then([&](const auto &known) {
            return compilation::selectContext(context, file,
                compilation::groupCandidates(std::move(candidates), file.path, known));
          });
        }).transform([&](auto command) {
          return CompilationContext{project, {std::move(command)}};
        });
      });
}
}
