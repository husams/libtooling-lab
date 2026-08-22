#pragma once

#include <clang/Tooling/CompilationDatabase.h>

#include "tooling/ProjectImport.h"

#include <expected>
#include <memory>
#include <string>

namespace facts {

std::expected<std::unique_ptr<clang::tooling::CompilationDatabase>, std::string>
loadStoredCompilationDatabase(std::string databasePath);

} // namespace facts
