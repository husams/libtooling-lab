#pragma once
#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace facts::apis::index {
template <typename T> using Result = std::expected<T, std::string>;
struct Query {
  std::string qualifiedName;
  std::optional<std::string> kind, usr, repository, component;
  unsigned limit = 50;
  std::optional<std::string> cursor;
};
struct Symbol {
  std::string qualifiedName, kind, usr;
  std::int64_t fileId = 0;
  std::string path, repository, clone, component;
  bool definition = false;
  std::int64_t position = 0;
};
struct Page {
  std::vector<Symbol> items;
  std::optional<std::string> nextCursor;
  std::int64_t generation = 0;
};
struct RefreshResult {
  std::int64_t generation = 0;
  std::int64_t symbols = 0;
  std::size_t sources = 0;
  std::size_t missingSources = 0;
};
// Blocking database work: invoke on the server's background executor.
// A nonempty configuredSources list supplies all configuration-resolved paths.
Result<RefreshResult> refresh(const std::filesystem::path &project,
    const std::vector<std::filesystem::path> &configuredSources = {},
    const std::filesystem::path &factsBase = {});
Result<Page> search(const std::filesystem::path &project, const Query &query);
Result<std::optional<RefreshResult>> published(const std::filesystem::path &project);
}
