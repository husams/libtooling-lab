#include "commands/Import.h"

#include "commands/IncludedFiles.h"

#include "cli/Verbose.h"
#include "storage/FileManager.h"
#include "tooling/CompilationFiles.h"
#include "tooling/ProjectImport.h"

#include <clang/Tooling/CompilationDatabase.h>

#include <algorithm>
#include <cstddef>

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

std::expected<std::size_t, std::string>
registeredFileCount(FileManager &files) {
  return files.fileCount().transform_error([](std::error_code error) {
    return "cannot read the file registry: " + error.message();
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
        return discoverIncludedFiles(database, selected)
            .and_then([&, identities = std::move(discovered.files)](
                          std::vector<std::string> included) mutable {
              std::ranges::move(included, std::back_inserter(identities));
              std::ranges::sort(identities);
              identities.erase(std::ranges::unique(identities).begin(),
                               identities.end());
              return files.addBulk(identities)
                  .transform_error([](std::error_code error) {
                    return "cannot register compilation files: " +
                           error.message();
                  });
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
  // Storing the compile commands registers the sources themselves, so the
  // reported figure is what the whole import added to the registry: a repeated
  // import of an unchanged project adds nothing and says so.
  return cli::runStage(options.verbosity, "import", "read file registry",
                       [&] { return registeredFileCount(files); })
      .and_then([&](std::size_t before) {
        return cli::runStage(
                   options.verbosity, "import", "store compile commands",
                   [&] {
                     return importProjectConfiguration(
                         files, *database, options.sources, importOptions);
                   })
            .and_then([&](const ProjectImportResult &result) {
              reportDiagnostics(result.diagnostics);
              return cli::runStage(options.verbosity, "import",
                                   "register files",
                                   [&] {
                                     return registerFiles(files, *database,
                                                          options.sources);
                                   })
                  .and_then([&](std::size_t) {
                    return registeredFileCount(files);
                  })
                  .transform([&](std::size_t after) {
                    std::cout << "Imported " << result.importedFiles
                              << " compile command(s)\n";
                    std::cout << "Registered " << (after - before)
                              << " file(s)\n";
                    return 0;
                  });
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
