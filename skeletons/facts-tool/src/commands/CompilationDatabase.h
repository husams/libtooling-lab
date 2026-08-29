#pragma once

#include <clang/Tooling/ArgumentsAdjusters.h>
#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/CompilationDatabase.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace facts::commands {

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
