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

Result<std::filesystem::path> componentRoot(const Component &component);
} // namespace facts::catalog
