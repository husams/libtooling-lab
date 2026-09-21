#pragma once

#include <clang/Tooling/CompilationDatabase.h>
#include <expected>
#include <filesystem>
#include <memory>
#include <string>

namespace facts {
// Resolve relative command directories against compile_commands.json before
// selecting sources or expanding response files. No process cwd is changed.
std::expected<std::unique_ptr<clang::tooling::CompilationDatabase>, std::string>
loadImportCompilationDatabase(const std::filesystem::path &directory);
}
