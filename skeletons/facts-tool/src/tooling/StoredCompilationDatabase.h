#pragma once

#include <clang/Tooling/CompilationDatabase.h>

#include "tooling/ProjectImport.h"

#include <expected>
#include <memory>
#include <span>
#include <string>

namespace facts {

std::expected<std::unique_ptr<clang::tooling::CompilationDatabase>, std::string>
loadStoredCompilationDatabase(
    std::string databasePath,
    std::span<const std::string> requestedSources = {});

} // namespace facts
