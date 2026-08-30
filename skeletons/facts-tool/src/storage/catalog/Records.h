#pragma once

#include "storage/ProjectConfiguration.h"
#include "storage/catalog/Database.h"

namespace facts::catalog {

struct Repository {
  std::int64_t id = 0;
  std::string name;
  std::string kind;
  std::string remote;
  std::optional<std::int64_t> activeCloneId;
  std::string activePath;
  std::int64_t components = 0;
  std::int64_t clones = 0;
};

struct Component {
  ProjectComponent value;
  std::optional<ProjectClone> clone;
  std::string repository;
  std::int64_t files = 0;
};

struct Directory {
  std::int64_t id = 0;
  std::int64_t componentId = 0;
  std::string component;
  std::string path;
  std::int64_t files = 0;
};

struct File {
  std::int64_t id = 0;
  std::int64_t directoryId = 0;
  ProjectComponent component;
  std::optional<ProjectClone> clone;
  std::string componentName;
  std::string directory;
  std::string name;
  std::string compileOptions;
  std::string driver;
  std::string workingDirectory;
  bool argsOverridden = false;
  bool indexed = false;
};

Result<std::filesystem::path> componentRoot(const Component &component);
Result<std::filesystem::path> filePath(const File &file);
std::string relativeFilePath(const File &file);
} // namespace facts::catalog
