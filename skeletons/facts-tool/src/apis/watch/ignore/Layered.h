#pragma once
#include "apis/watch/ignore/Rules.h"
#include <map>

namespace facts::apis::watch::ignore {
struct Layered {
  std::map<std::filesystem::path, std::optional<Rules>> files;
  std::map<std::string, bool> decisions;
  std::expected<bool, std::string>
  excludes(git_repository *repository, const std::filesystem::path &root,
           const std::filesystem::path &relative, bool directory);
  std::expected<bool, std::string>
  direct(git_repository *repository, const std::filesystem::path &root,
         const std::filesystem::path &relative, bool directory);
};
std::expected<bool, std::string>
matchesTree(git_repository *repository, const std::filesystem::path &relative,
            bool directory);
}
