#include "apis/watch/ignore/Layered.h"

namespace facts::apis::watch::ignore {
std::expected<bool, std::string>
matchesTree(git_repository *repository, const std::filesystem::path &relative,
            bool directory) {
  std::filesystem::path prefix;
  for (const auto &part : relative) {
    prefix /= part;
    const bool isDirectory = prefix != relative || directory;
    auto result = matches(repository,
        prefix.generic_string() + (isDirectory ? "/" : ""));
    if (!result || *result) return result;
  }
  return false;
}

std::expected<bool, std::string>
Layered::excludes(git_repository *repository, const std::filesystem::path &root,
                  const std::filesystem::path &relative, bool directory) {
  std::filesystem::path prefix;
  for (const auto &part : relative) {
    prefix /= part;
    const bool isDirectory = prefix != relative || directory;
    const auto key = prefix.generic_string() + (isDirectory ? "/" : "");
    auto found = decisions.find(key);
    if (found == decisions.end()) {
      auto result = direct(repository, root, prefix, isDirectory);
      if (!result) return std::unexpected(result.error());
      found = decisions.emplace(key, *result).first;
    }
    if (found->second) return true;
  }
  return false;
}

std::expected<bool, std::string>
Layered::direct(git_repository *repository, const std::filesystem::path &root,
                const std::filesystem::path &relative, bool directory) {
  const auto candidate = root / relative;
  auto parent = candidate.parent_path();
  while (true) {
    const auto file = parent / ".gitignore";
    auto found = files.find(file);
    if (found == files.end()) {
      auto rules = readRules(file);
      if (!rules) return std::unexpected(rules.error());
      found = files.emplace(file, std::move(*rules)).first;
    }
    if (found->second) {
      const auto name = candidate.lexically_relative(parent).generic_string();
      const auto result = matchRules(*found->second,
                                     name + (directory ? "/" : ""));
      if (!result) return std::unexpected(result.error());
      if (*result) return **result;
    }
    if (parent == root) break;
    if (parent == parent.parent_path())
      return std::unexpected("ignore path is outside clone root");
    parent = parent.parent_path();
  }
  return matches(repository, relative.generic_string() + (directory ? "/" : ""));
}
}
