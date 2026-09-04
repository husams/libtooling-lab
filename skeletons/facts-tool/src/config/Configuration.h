#pragma once

#include <expected>
#include <filesystem>
#include <string>
#include <vector>

namespace facts::config {

struct Request {
  std::string selector;
  std::string direct;
  bool create = false;
};

struct Resolved {
  std::filesystem::path database;
  std::filesystem::path projectRoot;
  std::filesystem::path storageRoot;
  std::string templateText;
  std::string source;
  bool generated = true;
  std::vector<std::string> extraArguments;
  std::string diagnostics;
};

std::expected<Resolved, std::string> resolve(const Request &request);
std::expected<Resolved, std::string>
readYaml(const std::filesystem::path &path, Resolved result);
std::expected<std::filesystem::path, std::string>
renderDatabasePath(const Resolved &value);
std::expected<void, std::string>
ensureOwnedDatabase(const Resolved &resolved);

} // namespace facts::config
