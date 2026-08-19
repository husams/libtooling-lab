#pragma once

#include <clang/Tooling/CompilationDatabase.h>

#include <expected>
#include <memory>
#include <optional>
#include <string>

namespace facts {

std::expected<std::unique_ptr<clang::tooling::CompilationDatabase>, std::string>
loadStoredCompilationDatabase(std::string databasePath);

void configureStoredCompilationDatabase(std::string databasePath);

std::optional<std::string> storedCompilationDatabaseError();

} // namespace facts
