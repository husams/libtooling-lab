#include "commands/Dependency.h"

#include "ast/visitors/IncludeVisitor.h"
#include "cli/Options.h"
#include "model/Dependency.h"
#include "platform/PlatformFlags.h"
#include "storage/DependencyDatabase.h"
#include "storage/FileManager.h"
#include "tooling/CompilationFiles.h"
#include "tooling/StoredCompilationDatabase.h"

#include <clang/Tooling/CompilationDatabase.h>
#include <clang/Tooling/Tooling.h>

#include <algorithm>
#include <filesystem>
#include <memory>
#include <string>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>

namespace facts::commands {
namespace {

using CompilationDatabase = clang::tooling::CompilationDatabase;
using CompilationDatabasePtr = std::unique_ptr<CompilationDatabase>;

struct StoredGraph {
  std::vector<FileId> visitedSources;
  std::vector<DependencyEdge> edges;
};

std::string normalized(std::string_view path) {
  return std::filesystem::canonical(path).lexically_normal().string();
}

template <typename Value>
void sortUnique(std::vector<Value> &values) {
  std::ranges::sort(values);
  values.erase(std::unique(values.begin(), values.end()), values.end());
}

std::expected<CompilationDatabasePtr, std::string>
requireStoredCommands(CompilationDatabasePtr database) {
  if (database->getAllCompileCommands().empty()) {
    return std::unexpected(
        "project configuration contains no stored compile commands");
  }
  return database;
}

std::expected<std::vector<std::string>, std::string>
registerFiles(FileManager &files, const CompilationDatabase &database,
              const std::vector<std::string> &sources) {
  return discoverCompilationFiles(database, sources)
      .transform_error([](std::error_code error) {
        return "cannot discover compilation files: " + error.message();
      })
      .and_then([&](std::vector<std::string> paths) {
        return files.addBulk(paths)
            .transform_error([](std::error_code error) {
              return "cannot register compilation files: " + error.message();
            })
            .transform([paths = std::move(paths)]() mutable {
              return std::move(paths);
            });
      });
}

std::expected<IncludeGraphFacts, std::string>
collectIncludes(const CompilationDatabase &database,
                const std::vector<std::string> &sources) {
  IncludeGraphFacts facts;
  clang::tooling::ClangTool tool(database, sources);
  addPlatformFlags(tool);
  const auto result = tool.run(createIncludeVisitorFactory(facts).get());
  if (result != 0) {
    return std::unexpected("dependency analysis failed while parsing sources");
  }
  return facts;
}

std::expected<std::vector<std::string>, std::string>
registerIncludeFiles(FileManager &files, std::vector<std::string> registered,
                     const IncludeGraphFacts &facts) {
  auto included = facts.visitedSources;
  sortUnique(included);
  return files.addBulk(included)
      .transform_error([](std::error_code error) {
        return "cannot register included files: " + error.message();
      })
      .transform([registered = std::move(registered),
                  included = std::move(included)]() mutable {
        std::ranges::move(included, std::back_inserter(registered));
        sortUnique(registered);
        return registered;
      });
}

std::expected<std::unordered_map<std::string, FileId>, std::string>
resolveRegisteredFiles(FileManager &files,
                       const std::vector<std::string> &registered) {
  std::unordered_map<std::string, FileId> ids;
  for (const auto &path : registered) {
    auto id = files.getId(path);
    if (!id) {
      return std::unexpected("cannot resolve registered file: " +
                             id.error().message());
    }
    ids.emplace(normalized(path), *id);
  }
  return ids;
}

std::expected<FileId, std::string>
requireRegistered(const std::unordered_map<std::string, FileId> &registered,
                  const std::string &path) {
  auto id = registered.find(normalized(path));
  if (id == registered.end()) {
    return std::unexpected("include endpoint is not registered: " + path);
  }
  return id->second;
}

std::expected<StoredGraph, std::string>
toStoredGraph(const IncludeGraphFacts &facts,
              const std::unordered_map<std::string, FileId> &registered) {
  StoredGraph graph;
  for (const auto &source : facts.visitedSources) {
    auto id = requireRegistered(registered, source);
    if (!id) {
      return std::unexpected(id.error());
    }
    graph.visitedSources.push_back(*id);
  }
  for (const auto &edge : facts.edges) {
    auto source = requireRegistered(registered, edge.source);
    if (!source) {
      return std::unexpected(source.error());
    }
    auto destination = requireRegistered(registered, edge.destination);
    if (!destination) {
      return std::unexpected(destination.error());
    }
    graph.edges.push_back({*source, *destination});
  }
  sortUnique(graph.visitedSources);
  sortUnique(graph.edges);
  return graph;
}

std::expected<int, std::string> analyse(const cli::DependencyOptions &options,
                                        CompilationDatabasePtr database) {
  FileManager files(options.configuration);
  return registerFiles(files, *database, options.sources)
      .and_then([&](std::vector<std::string> registered) {
        return collectIncludes(*database, options.sources)
            .and_then([&, registered = std::move(registered)](
                          IncludeGraphFacts facts) mutable {
              return registerIncludeFiles(files, std::move(registered), facts)
                  .and_then([&, facts = std::move(facts)](
                                std::vector<std::string> paths) mutable {
                    return resolveRegisteredFiles(files, paths)
                        .and_then([&, facts = std::move(facts)](auto ids) {
                          return toStoredGraph(facts, ids);
                        });
                  })
                  .and_then([&](StoredGraph graph) {
                    return replaceDependencies(options.configuration,
                                               graph.visitedSources,
                                               graph.edges)
                        .transform_error([](std::error_code error) {
                          return "cannot persist dependency graph: " +
                                 error.message();
                        })
                        .transform([] { return 0; });
                  });
            });
      });
}

} // namespace

std::expected<int, std::string>
runDependency(const cli::DependencyOptions &options) {
  if (options.sources.empty()) {
    return std::unexpected(
        "dependency analysis requires at least one translation-unit source");
  }
  return loadStoredCompilationDatabase(options.configuration)
      .and_then(requireStoredCommands)
      .and_then([&](CompilationDatabasePtr database) {
        return analyse(options, std::move(database));
      });
}

} // namespace facts::commands
