#pragma once
#include "apis/config/Settings.h"
#include "config/Configuration.h"
#include "storage/ProjectConfiguration.h"
#include <expected>
#include <nlohmann/json.hpp>
#include <optional>

namespace facts::apis::domain {
struct Error {
  unsigned status = 500;
  std::string code;
  std::string message;
  nlohmann::json details = nullptr;
};
template <class Value> using Result = std::expected<Value, Error>;
struct FileSelector {
  std::string path;
  std::optional<std::string> repo;
  std::optional<std::string> clone;
  std::optional<std::string> component;
};
struct Context {
  config::Resolved configuration;
  std::string configurationFile;
};
struct ResolvedFile {
  std::int64_t fileId = 0;
  std::filesystem::path path;
  std::filesystem::path facts;
  std::string repository;
  std::string component;
  std::optional<ProjectClone> clone;
  bool activeClone = true;
};
Result<Context> resolveContext(const Settings &settings);
Result<Context> workspaceContext(const Context &context, const std::filesystem::path &root);
Result<ResolvedFile> resolveFile(const Context &context,
                                 const FileSelector &selector, bool requireAvailable = true);
Result<std::vector<std::filesystem::path>> factSources(const Context &context);
} // namespace facts::apis::domain
