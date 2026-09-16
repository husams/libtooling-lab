#pragma once

#include <expected>
#include <filesystem>
#include <memory>
#include <string>

namespace clang {
class ASTUnit;
}

namespace facts::astcache::detail {

// Embed the parsed source buffers so a commit-scoped snapshot remains usable
// when its original source files are edited or removed from the working tree.
std::expected<void, std::string>
saveSerialized(clang::ASTUnit &unit, const std::filesystem::path &path);

std::unique_ptr<clang::ASTUnit>
loadSerialized(const std::filesystem::path &path,
               const std::filesystem::path &workingDirectory);

} // namespace facts::astcache::detail
