#include "storage/FileManager.h"

#include "storage/FileDatabase.h"

#include <algorithm>
#include <filesystem>
#include <ranges>
#include <utility>
#include <vector>

namespace facts {
namespace {

std::expected<std::filesystem::path, std::error_code>
absoluteIdentityPath(std::string_view path) {
  std::filesystem::path candidate{path};
  if (candidate.is_absolute()) {
    return candidate;
  }

  std::error_code error;
  auto current = std::filesystem::current_path(error);
  if (error) {
    return std::unexpected(error);
  }
  return current / candidate;
}

std::expected<std::string, std::error_code>
canonicalIdentity(std::string_view path) {
  return absoluteIdentityPath(path).and_then([](auto absolute) {
    std::error_code error;
    auto identity = std::filesystem::canonical(absolute, error);
    if (error) {
      return std::expected<std::string, std::error_code>{
          std::unexpected(error)};
    }
    return std::expected<std::string, std::error_code>{
        identity.lexically_normal().string()};
  });
}

std::expected<std::vector<std::string>, std::error_code>
canonicalIdentities(std::span<const std::string> paths,
                    std::string_view registryPath) {
  auto canonical = paths | std::views::transform(canonicalIdentity);
  std::vector<std::string> identities;
  identities.reserve(paths.size());
  for (auto identity : canonical) {
    if (!identity) {
      return std::unexpected(identity.error());
    }
    identities.push_back(std::move(*identity));
  }

  auto physicalFiles =
      std::move(identities) | std::views::filter([&](const auto &identity) {
        return identity != registryPath;
      });
  auto result = physicalFiles | std::ranges::to<std::vector>();
  std::ranges::sort(result);
  result.erase(std::ranges::unique(result).begin(), result.end());
  return result;
}

} // namespace

namespace {

std::string identityPath(const std::string &databasePath) {
  return std::filesystem::absolute(databasePath).lexically_normal().string();
}

} // namespace

FileManager::FileManager(std::string databasePath)
    : databasePath_(identityPath(databasePath)),
      database_(std::make_unique<FileDatabase>(databasePath_)) {}

FileManager::FileManager(std::string databasePath,
                         std::unique_ptr<FileDatabase> database)
    : databasePath_(std::move(databasePath)), database_(std::move(database)) {}

std::expected<std::unique_ptr<FileManager>, std::string>
FileManager::openReadOnly(std::string databasePath) {
  auto identity = identityPath(databasePath);
  return FileDatabase::openReadOnly(identity).transform(
      [&](std::unique_ptr<FileDatabase> database) {
        return std::unique_ptr<FileManager>(
            new FileManager(std::move(identity), std::move(database)));
      });
}

FileManager::~FileManager() = default;

std::expected<void, std::error_code>
FileManager::addBulk(std::span<const std::string> paths) {
  return canonicalIdentities(paths, databasePath_)
      .and_then([this](std::vector<std::string> identities) {
        return database_->addBulk(identities);
      });
}

std::expected<void, std::error_code> FileManager::replaceProjectConfiguration(
    const ProjectConfiguration &configuration) {
  return database_->replaceProjectConfiguration(configuration);
}

std::expected<void, std::error_code>
FileManager::switchActiveClone(std::string_view repositoryName,
                               std::string_view clonePathOrLabel) {
  return database_->switchActiveClone(repositoryName, clonePathOrLabel);
}

std::expected<void, std::error_code>
FileManager::addClone(std::string_view repositoryName,
                      const ProjectClone &clone, bool activate) {
  return database_->addClone(repositoryName, clone, activate);
}

std::expected<FileId, std::error_code>
FileManager::getId(std::string_view path) {
  const auto getCachedOrStored = [this](const std::string &identity) {
    if (const auto found = fileIds_.find(identity); found != fileIds_.end()) {
      return std::expected<FileId, std::error_code>{found->second};
    }
    return database_->getId(identity);
  };

  return canonicalIdentity(path).and_then([&](std::string identity) {
    return getCachedOrStored(identity).transform([&](FileId id) {
      fileIds_.try_emplace(std::move(identity), id);
      return id;
    });
  });
}

} // namespace facts
