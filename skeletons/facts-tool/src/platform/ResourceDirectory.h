#ifndef FACTS_TOOL_RESOURCEDIRECTORY_H
#define FACTS_TOOL_RESOURCEDIRECTORY_H

#include <expected>
#include <filesystem>
#include <string>
#include <vector>

namespace facts::platform {

struct ResourceDirectoryRequest {
  std::filesystem::path library;
  std::filesystem::path computed;
  unsigned clangMajor;
};

std::vector<std::filesystem::path>
resourceDirectoryCandidates(const ResourceDirectoryRequest &request);

std::expected<std::filesystem::path, std::string>
resolveResourceDirectory(const ResourceDirectoryRequest &request);

std::expected<std::filesystem::path, std::string>
resolveLinkedResourceDirectory();

} // namespace facts::platform

#endif // FACTS_TOOL_RESOURCEDIRECTORY_H
