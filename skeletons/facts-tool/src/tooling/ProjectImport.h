#ifndef FACTS_TOOL_TOOLING_PROJECT_IMPORT_H
#define FACTS_TOOL_TOOLING_PROJECT_IMPORT_H

#include "storage/ProjectConfiguration.h"

#include <clang/Tooling/CompilationDatabase.h>

#include <expected>
#include <span>
#include <string>
#include <vector>

namespace facts {

class FileManager;

struct ProjectImportOptions {
  std::string repositoryName = "facts-tool";
  std::string remoteUrl;
  std::string cloneLabel = "active";
  std::vector<ProjectComponent> components;
};

struct ProjectImportResult {
  std::size_t importedFiles = 0;
  std::size_t duplicateCommands = 0;
  std::vector<std::string> diagnostics;
};

std::expected<ProjectImportResult, std::string>
importProjectConfiguration(FileManager &files,
                           const clang::tooling::CompilationDatabase &database,
                           std::span<const std::string> fallbackSources,
                           const ProjectImportOptions &options);

} // namespace facts

#endif // FACTS_TOOL_TOOLING_PROJECT_IMPORT_H
