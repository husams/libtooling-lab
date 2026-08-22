#include "commands/Import.h"

#include "storage/FileManager.h"
#include "tooling/ProjectImport.h"

#include <clang/Tooling/CompilationDatabase.h>

#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
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

  std::string error;
  auto database = CompilationDatabase::loadFromDirectory(
      options.compilationDatabase, error);
  if (!database) {
    return std::unexpected("cannot load compilation database from " +
                           options.compilationDatabase + ": " + error);
  }
  return database;
}

std::expected<int, std::string> import(const cli::ImportOptions &options,
                                       std::vector<ProjectComponent> components,
                                       CompilationDatabasePtr database) {
  FileManager files(options.configuration);
  ProjectImportOptions importOptions;
  importOptions.components = std::move(components);
  return importProjectConfiguration(files, *database, options.sources,
                                    importOptions)
      .transform([](const ProjectImportResult &result) {
        for (const auto &diagnostic : result.diagnostics) {
          std::cerr << "facts-tool: " << diagnostic << '\n';
        }
        std::cout << "Imported " << result.importedFiles
                  << " compile command(s)\n";
        return 0;
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
