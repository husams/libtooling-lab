#pragma once

#include <clang/Tooling/ArgumentsAdjusters.h>
#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/CompilationDatabase.h>

#include <memory>
#include <filesystem>
#include <string>
#include <span>
#include <utility>
#include <vector>

namespace facts::commands {

inline std::vector<std::string>
normalizeSourceSelectors(std::span<const std::string> sources) {
  std::vector<std::string> normalized;
  normalized.reserve(sources.size());
  for (const auto &source : sources) {
    const std::filesystem::path path(source);
    normalized.push_back((path.is_absolute()
                              ? path
                              : std::filesystem::absolute(path))
                             .lexically_normal()
                             .string());
  }
  return normalized;
}

using CompilationDatabasePtr =
    std::unique_ptr<clang::tooling::CompilationDatabase>;

inline CompilationDatabasePtr
appendExtraArguments(CompilationDatabasePtr database,
                     const std::vector<std::string> &extraArguments) {
  if (extraArguments.empty()) {
    return database;
  }

  auto adjusted =
      std::make_unique<clang::tooling::ArgumentsAdjustingCompilations>(
          std::move(database));
  adjusted->appendArgumentsAdjuster(clang::tooling::getInsertArgumentAdjuster(
      extraArguments, clang::tooling::ArgumentInsertPosition::END));
  return adjusted;
}

} // namespace facts::commands
