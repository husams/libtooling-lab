#pragma once

#include <expected>
#include <filesystem>
#include <llvm/ADT/StringRef.h>
#include <string>

namespace facts::astcache::detail {
namespace fs = std::filesystem;
std::string digest(llvm::StringRef bytes);
std::expected<std::string, std::string> readDigest(const fs::path &path);
fs::path resolve(const fs::path &path, const fs::path &directory);
} // namespace facts::astcache::detail
