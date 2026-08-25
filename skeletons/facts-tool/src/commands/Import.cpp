#include "commands/Import.h"

#include "ast/visitors/IncludeVisitor.h"
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
std::vector<std::string> includedFiles(const CompilationDatabase &database,
                                       const std::vector<std::string> &sources,
                                       std::vector<std::string> &diagnostics) {
  auto configured = configurePlatformCompilationDatabase(database, sources);
  if (!configured) {
    diagnostics.push_back("cannot resolve included files: " +
                          configured.error());
    return {};
  }

  IncludeGraphFacts facts;
  clang::tooling::ClangTool tool(**configured, sources);
  if (tool.run(createIncludeVisitorFactory(facts).get()) != 0) {
    diagnostics.push_back(
        "some translation units did not preprocess cleanly; their included "
        "files may be missing from the registry");
  }
  sortUnique(facts.visitedSources);
  return std::move(facts.visitedSources);
}

// Import owns the file registry: every path a later command can resolve is
// discovered and stored here, so extraction only ever reads it.
std::expected<std::size_t, std::string>
registerFiles(FileManager &files, const CompilationDatabase &database,
              const std::vector<std::string> &sources) {
  return discoverCompilationFiles(database, sources)
      .and_then([&](CompilationFiles discovered) {
        auto selected = sources.empty() ? database.getAllFiles() : sources;
        auto included =
            includedFiles(database, selected, discovered.diagnostics);
        reportDiagnostics(discovered.diagnostics);
        std::ranges::move(included, std::back_inserter(discovered.files));
        sortUnique(discovered.files);
        return files.addBulk(discovered.files)
            .transform_error([](std::error_code error) {
              return "cannot register compilation files: " + error.message();
            })
            .transform([count = discovered.files.size()] { return count; });
      });
}

std::expected<int, std::string> import(const cli::ImportOptions &options,
                                       std::vector<ProjectComponent> components,
                                       CompilationDatabasePtr database) {
  FileManager files(options.configuration);
  ProjectImportOptions importOptions;
  importOptions.components = std::move(components);
  return importProjectConfiguration(files, *database, options.sources,
                                    importOptions)
      .and_then([&](const ProjectImportResult &result) {
        reportDiagnostics(result.diagnostics);
        return registerFiles(files, *database, options.sources)
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
  return parseComponents(options.components)
      .and_then([&](std::vector<ProjectComponent> components) {
        return loadCompilationDatabase(options).and_then(
            [&](CompilationDatabasePtr database) {
              return import(options, std::move(components),
                            std::move(database));
            });
      });
}

} // namespace facts::commands
