#ifndef FACTS_TOOL_STORAGE_PROJECT_CONFIGURATION_H
#define FACTS_TOOL_STORAGE_PROJECT_CONFIGURATION_H

#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace facts {

struct ProjectClone {
  std::int64_t id = 0;
  std::int64_t repositoryId = 0;
  std::string path;
  std::string label;
};

struct ProjectComponent {
  std::int64_t id = 0;
  std::string name;
  std::string path;
  std::string kind = "repo";
  std::optional<std::string> version;
  std::optional<std::int64_t> repositoryId;
};

struct ProjectFile {
  std::string componentPath;
  std::string directory;
  std::string name;
  std::string driver;
  std::string workingDirectory;
  std::string compileOptions;
};

struct ProjectConfiguration {
  std::string repositoryName;
  std::string remoteUrl;
  ProjectClone activeClone;
  std::vector<ProjectComponent> components;
  std::vector<ProjectFile> files;
};

struct StoredFile {
  std::int64_t id = 0;
  ProjectComponent component;
  std::string directory;
  std::string name;
  std::string driver;
  std::string workingDirectory;
  std::string compileOptions;
};

std::filesystem::path effectiveComponentRoot(
    const ProjectComponent &component,
    const std::optional<ProjectClone> &activeClone = std::nullopt);

std::filesystem::path
fullProjectFilePath(const ProjectComponent &component,
                    const std::optional<ProjectClone> &clone,
                    std::string_view directory, std::string_view name);

std::optional<std::size_t>
selectOwningComponent(std::span<const ProjectComponent> components,
                      const ProjectClone &activeClone,
                      const std::filesystem::path &source);

// Names the repository, clone, component or file field that makes a
// configuration unstorable, so a caller reports the cause instead of a bare
// "Invalid argument".
std::expected<void, std::string>
validateProjectConfiguration(const ProjectConfiguration &configuration);

} // namespace facts

#endif // FACTS_TOOL_STORAGE_PROJECT_CONFIGURATION_H
