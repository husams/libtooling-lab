#pragma once

#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace facts::config {

struct Request {
  std::string selector;
  std::string direct;
  bool create = false;
  bool loadDefaults = true;
};

struct Resolved {
  std::filesystem::path database;
  std::filesystem::path projectRoot;
  std::filesystem::path storageRoot;
  std::string templateText;
  std::string factsTemplate;
  std::string source;
  bool generated = true;
  std::vector<std::string> extraArguments;
  std::string diagnostics;
  std::string storageRootSource;
  std::string templateSource;
  std::string factsTemplateSource;
  std::string extraArgumentsSource;
  std::vector<std::string> discovery;
};

// One parsed YAML file, before precedence/merge is applied. A field is
// nullopt when the file omitted that key.
struct Tier {
  std::filesystem::path path;
  std::optional<std::string> confRoot;
  std::optional<std::string> confTemplate;
  std::optional<std::string> factsTemplate;
  std::optional<std::vector<std::string>> extraArgs;
};

// Parses one YAML file. When applyPathSettings is false (a direct --conf /
// FACTS_TOOL_CONF override is active) conf_root/conf_template/facts_template
// are left unpopulated and unvalidated, but the document structure and
// extra_args are always validated.
std::expected<Tier, std::string> readTier(const std::filesystem::path &path,
                                          bool applyPathSettings);

std::expected<Resolved, std::string> resolve(const Request &request,
                                           Resolved *partial = nullptr);
std::expected<std::filesystem::path, std::string>
renderDatabasePath(const Resolved &value);
std::expected<std::filesystem::path, std::string>
renderFactsPath(const Resolved &value, const std::vector<std::string> &sources);
std::expected<void, std::string>
ensureOwnedDatabase(const Resolved &resolved);

} // namespace facts::config
