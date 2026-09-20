#pragma once
#include "apis/watch/ignore/Repository.h"
#include <optional>

namespace facts::apis::watch::ignore {
struct Rules {
  Repository included{nullptr, git_repository_free};
  Repository excluded{nullptr, git_repository_free};
};
std::expected<std::optional<Rules>, std::string>
readRules(const std::filesystem::path &file);
std::expected<std::optional<bool>, std::string>
matchRules(const Rules &rules, const std::string &relative);
}
