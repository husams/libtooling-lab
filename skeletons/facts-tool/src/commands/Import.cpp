#include "commands/Import.h"

#include "commands/CompilationDatabase.h"
#include "commands/ExtraArguments.h"
#include "commands/ConfigurationSupport.h"
#include "commands/IncludedFiles.h"

#include "cli/Verbose.h"
#include "platform/PlatformFlags.h"
#include "storage/FileManager.h"
#include "tooling/CompilationFiles.h"
#include "tooling/ProjectImport.h"
#include "tooling/StoredCompilationDatabase.h"

#include <clang/Tooling/CommonOptionsParser.h>
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

std::expected<CompilationDatabasePtr, std::string>
loadJsonCompilationDatabase(const std::string &directory) {
  std::string error;
  auto database = CompilationDatabase::loadFromDirectory(directory, error);
  if (!database) {
    return std::unexpected("cannot load compilation database from " +
                           directory + ": " + error);
  }
  return database;
}

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
loadCompilationDatabase(const cli::ImportOptions &options,
                       const std::vector<std::string> &arguments) {
  if (options.compilationDatabase.empty()) {
    if (options.sources.empty())
      return std::unexpected("import requires --compilation-database or at least one source");
    return std::make_unique<clang::tooling::FixedCompilationDatabase>(
        std::filesystem::current_path().string(), arguments);
  }
  return loadJsonCompilationDatabase(options.compilationDatabase)
      .transform([&](CompilationDatabasePtr database) {
        return appendExtraArguments(std::move(database), arguments);
      });
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

// Extraction discovers the files it needs through the compile commands this
// import has just stored, not through the ones it was handed: storing a
// command sanitizes its flags and rewrites its paths, and a set discovered
// before that round trip is not the set discovered after it.
std::expected<std::vector<std::string>, std::string>
discoverRegistryFiles(const CompilationDatabase &stored,
                      const std::vector<std::string> &sources) {
  return discoverCompilationFiles(stored, {})
      .and_then([&](CompilationFiles discovered) {
        reportDiagnostics(discovered.diagnostics);
        return discoverIncludedFiles(stored, sources)
            .transform([identities = std::move(discovered.files)](
                           std::vector<std::string> included) mutable {
              std::ranges::move(included, std::back_inserter(identities));
              std::ranges::sort(identities);
              identities.erase(std::ranges::unique(identities).begin(),
                               identities.end());
              return std::move(identities);
            });
      });
}

// Registration is only worth anything if the registry answers the question
// extraction will ask, so import asks it here: every discovered identity is
// resolved back through the same lookup, and an import that cannot satisfy it
// fails instead of leaving a database extraction will reject later.
std::expected<void, std::string>
requireResolvableIdentities(FileManager &files,
                            const std::vector<std::string> &identities) {
  const auto missing = std::ranges::find_if(
      identities, [&](const auto &identity) { return !files.getId(identity); });
  return missing == identities.end()
             ? std::expected<void, std::string>{}
             : std::expected<void, std::string>{std::unexpected(
                   "the file registry is incomplete after import; it cannot "
                   "resolve " +
                   *missing)};
}

// Import owns the file registry: every path a later command can resolve is
// discovered and stored here, so extraction only ever reads it.
std::expected<std::size_t, std::string>
registerFiles(FileManager &files, const CompilationDatabase &stored,
              const std::vector<std::string> &sources) {
  return discoverRegistryFiles(stored, sources)
      .and_then([&](std::vector<std::string> identities) {
        return files.addBulk(identities)
            .transform_error([](std::error_code error) {
              return "cannot register compilation files: " + error.message();
            })
            .and_then([&](std::size_t added) {
              return requireResolvableIdentities(files, identities)
                  .transform([added] { return added; });
            });
      })
      .and_then([&](std::size_t added) {
        return files.markRegistryComplete(platformFingerprint())
            .transform_error([](std::error_code error) {
              return "cannot record the completed file registry: " +
                     error.message();
            })
            .transform([added] { return added; });
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
  auto sources = options.sources.empty() ? database->getAllFiles()
                                         : options.sources;
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
              return cli::runStage(
                         options.verbosity, "import", "register files",
                         [&] {
                           return loadStoredCompilationDatabase(
                                      options.configuration)
                               .transform([&](CompilationDatabasePtr stored) {
                                 return appendExtraArguments(
                                     std::move(stored),
                                     options.defaultExtraArguments);
                               })
                               .and_then([&](CompilationDatabasePtr applied) {
                                 return registerFiles(files, *applied, sources);
                               });
                         })
                  .and_then(
                      [&](std::size_t) { return registeredFileCount(files); })
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
  auto resolved = loadConfiguration(options.configuration,
                                    options.configurationFile, false);
  if (!resolved) return std::unexpected(resolved.error());
  auto configured = options;
  configured.configuration = resolved->database.string();
  return cli::runStage(configured.verbosity, "import", "parse components",
                       [&] { return parseComponents(configured.components); })
      .and_then([&](std::vector<ProjectComponent> components) {
        return tokenizeExtraArguments(configured.extraArguments)
            .and_then([&](auto explicitArguments) {
              return cli::runStage(
                         configured.verbosity, "import",
                         "load compilation database", [&] {
                           return loadCompilationDatabase(configured,
                                                          explicitArguments);
                         })
                  .and_then([&](CompilationDatabasePtr database) {
                    if (resolved->generated) {
                      auto owned = config::ensureOwnedDatabase(*resolved);
                      if (!owned)
                        return std::expected<int, std::string>(
                            std::unexpected(owned.error()));
                    }
                    return import(configured, std::move(components),
                                  std::move(database));
                  });
            });
      });
}

} // namespace facts::commands
