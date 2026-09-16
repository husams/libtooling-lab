#pragma once

#include "model/AstCache.h"

#include <expected>
#include <filesystem>
#include <map>
#include <optional>
#include <string>

namespace clang {
class CompilerInstance;
}

namespace facts::astcache::detail {
struct RevisionObservations {
  std::map<std::filesystem::path, std::optional<Revision>> directories;
  std::map<std::string, Revision> repositories;
  std::optional<std::string> error;
};

// Observations must outlive the frontend action that receives the callbacks.
void attachRevisionObserver(clang::CompilerInstance &compiler,
                            const std::filesystem::path &workingDirectory,
                            RevisionObservations &observations);
std::expected<void, std::string>
validateObservedRevisions(const RevisionObservations &observations);
} // namespace facts::astcache::detail
