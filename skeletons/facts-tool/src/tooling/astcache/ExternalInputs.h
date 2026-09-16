#pragma once

#include <expected>
#include <filesystem>
#include <llvm/Support/JSON.h>
#include <string>

namespace clang::tooling {
struct CompileCommand;
}

namespace facts::astcache::detail {
std::expected<llvm::json::Array, std::string>
externalInputs(const clang::tooling::CompileCommand &command,
               const std::filesystem::path &cwd);
} // namespace facts::astcache::detail
