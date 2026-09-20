#pragma once
#include <git2.h>
#include <expected>
#include <filesystem>
#include <memory>
#include <string>

namespace facts::apis::watch::ignore {
using Repository = std::unique_ptr<git_repository, decltype(&git_repository_free)>;
using Index = std::unique_ptr<git_index, decltype(&git_index_free)>;
std::string error(const std::string &operation);
std::expected<Repository, std::string>
memoryRepository(const std::filesystem::path &workdir = {});
std::expected<Repository, std::string>
openRepository(const std::filesystem::path &root);
std::expected<bool, std::string>
matches(git_repository *repository, const std::string &relative);
}
