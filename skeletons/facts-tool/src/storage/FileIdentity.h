#ifndef FACTS_TOOL_STORAGE_FILE_IDENTITY_H
#define FACTS_TOOL_STORAGE_FILE_IDENTITY_H

#include "storage/ProjectConfiguration.h"

#include <cstdint>
#include <expected>
#include <filesystem>
#include <span>
#include <string>
#include <system_error>
#include <vector>

struct sqlite3;

namespace facts {

struct FileIdentity {
  std::int64_t componentId;
  std::string directory;
  std::string name;
};

struct FileIdentityContext {
  std::vector<ProjectComponent> components;
  ProjectClone clone;
};

std::expected<FileIdentityContext, std::error_code>
loadFileIdentityContext(sqlite3 *database);

std::expected<FileIdentity, std::error_code>
identifyFile(std::span<const ProjectComponent> components,
             const ProjectClone &clone, std::filesystem::path source);

std::expected<std::vector<FileIdentity>, std::error_code>
identifyFiles(const FileIdentityContext &context,
              std::span<const std::string> sources);

} // namespace facts

#endif // FACTS_TOOL_STORAGE_FILE_IDENTITY_H
