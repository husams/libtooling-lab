#include "commands/Import.h"

#include "ast/visitors/IncludeVisitor.h"
#include "cli/Verbose.h"
#include "platform/PlatformFlags.h"
#include "storage/FileManager.h"
#include "tooling/CompilationFiles.h"
#include "tooling/ProjectImport.h"

#include <clang/Tooling/CompilationDatabase.h>
#include <clang/Tooling/Tooling.h>

#include <algorithm>

#include <filesystem>
#include <iostream>
#include <iterator>
#include <memory>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace facts::commands {
namespace {

using CompilationDatabase = clang::tooling::CompilationDatabase;
using CompilationDatabasePtr = std::unique_ptr<CompilationDatabase>;

std::expected<ProjectComponent, std::string>
parseComponent(const std::string &specification) {
  const auto separator = specification.find('=');
  if (separator == std::string::npos || separator == 0 ||
      separator + 1 == specification.size()) {
    return std::unexpected("--component requires name=path: " + specification);
  }
  return ProjectComponent{.name = specification.substr(0, separator),
                          .path = specification.substr(separator + 1),
                          .kind = "repo"};
}

std::expected<std::vector<ProjectComponent>, std::string>
parseComponents(const std::vector<std::string> &specifications) {
  std::vector<ProjectComponent> components;
  components.reserve(specifications.size());
  for (const auto &specification : specifications) {
    auto component = parseComponent(specification);
    if (!component) {
      return std::unexpected(component.error());
    }
    components.push_back(std::move(*component));
  }
  return components;
}

std::expected<CompilationDatabasePtr, std::string>
loadCompilationDatabase(const cli::ImportOptions &options) {
  if (options.compilationDatabase.empty()) {
    if (options.sources.empty()) {
      return std::unexpected(
          "import requires --compilation-database or at least one source");
    }
    return std::make_unique<clang::tooling::FixedCompilationDatabase>(
        std::filesystem::current_path().string(), options.extraArguments);
  }
  if (!options.extraArguments.empty()) {
    return std::unexpected(
        "--extra-arg cannot be used with --compilation-database");
  }

  std::string error;
  auto database = CompilationDatabase::loadFromDirectory(
      options.compilationDatabase, error);
  if (!database) {
    return std::unexpected("cannot load compilation database from " +
                           options.compilationDatabase + ": " + error);
  }
  return database;
}

void reportDiagnostics(const std::vector<std::string> &diagnostics) {
  for (const auto &diagnostic : diagnostics) {
    std::cerr << "facts-tool: " << diagnostic << '\n';
  }
}

void sortUnique(std::vector<std::string> &values) {
  std::ranges::sort(values);
  values.erase(std::ranges::unique(values).begin(), values.end());
}

// Preprocessing each translation unit yields exactly the files extraction will
// later resolve, including the system headers that carry external targets.
// Extraction can only consume a complete registry, so a translation unit that
// fails to preprocess fails the import rather than leaving a silent gap.
std::expected<std::vector<std::string>, std::string>
includedFiles(const CompilationDatabase &database,
              const std::vector<std::string> &sources) {
  return configurePlatformCompilationDatabase(database, sources)
      .transform_error([](std::string error) {
        return "cannot resolve included files: " + std::move(error);
      })
      .and_then([&](auto configured)
                    -> std::expected<std::vector<std::string>, std::string> {
        IncludeGraphFacts facts;
        clang::tooling::ClangTool tool(*configured, sources);
        if (tool.run(createIncludeVisitorFactory(facts).get()) != 0) {
          return std::unexpected(
              "cannot enumerate included files: at least one translation unit "
              "failed to preprocess; fix the compile commands and import "
              "again");
        }
        sortUnique(facts.visitedSources);
        return std::move(facts.visitedSources);
      });
}

// Import owns the file registry: every path a later command can resolve is
// discovered and stored here, so extraction only ever reads it.
std::expected<std::size_t, std::string>
registerFiles(FileManager &files, const CompilationDatabase &database,
              const std::vector<std::string> &sources) {
  return discoverCompilationFiles(database, sources)
      .and_then([&](CompilationFiles discovered) {
        reportDiagnostics(discovered.diagnostics);
        const auto selected =
            sources.empty() ? database.getAllFiles() : sources;
        return includedFiles(database, selected)
            .and_then([&, identities = std::move(discovered.files)](
                          std::vector<std::string> included) mutable {
              std::ranges::move(included, std::back_inserter(identities));
              sortUnique(identities);
              return files.addBulk(identities)
                  .transform_error([](std::error_code error) {
                    return "cannot register compilation files: " +
                           error.message();
                  })
                  .transform([count = identities.size()] { return count; });
            });
      });
}

std::expected<int, std::string> import(const cli::ImportOptions &options,
                                       std::vector<ProjectComponent> components,
                                       CompilationDatabasePtr database) {
  cli::logVerbose(options.verbosity, 2,
                  "facts-tool: import: requested_sources={}, components={}",
                  options.sources.size(), components.size());
  cli::logVerbose(options.verbosity, 1,
                  "facts-tool: import: open project database");
  FileManager files(options.configuration);
  ProjectImportOptions importOptions;
  importOptions.components = std::move(components);
  return cli::runStage(options.verbosity, "import", "store compile commands",
                       [&] {
                         return importProjectConfiguration(
                             files, *database, options.sources, importOptions);
                       })
      .and_then([&](const ProjectImportResult &result) {
        reportDiagnostics(result.diagnostics);
        return cli::runStage(options.verbosity, "import", "register files",
                             [&] {
                               return registerFiles(files, *database,
                                                    options.sources);
                             })
            .transform([&](std::size_t registered) {
              std::cout << "Imported " << result.importedFiles
                        << " compile command(s)\n";
              std::cout << "Registered " << registered << " file(s)\n";
              return 0;
            });
      });
}

} // namespace

std::expected<int, std::string> runImport(const cli::ImportOptions &options) {
  return cli::runStage(options.verbosity, "import", "parse components",
                       [&] { return parseComponents(options.components); })
      .and_then([&](std::vector<ProjectComponent> components) {
        return cli::runStage(options.verbosity, "import",
                             "load compilation database",
                             [&] { return loadCompilationDatabase(options); })
            .and_then([&](CompilationDatabasePtr database) {
              return import(options, std::move(components),
                            std::move(database));
            });
      });
}

} // namespace facts::commands
