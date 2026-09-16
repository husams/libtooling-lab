#pragma once

#include <expected>
#include <filesystem>
#include <llvm/Support/JSON.h>
#include <string>

namespace facts::astcache::detail {
namespace fs = std::filesystem;
inline constexpr int Schema = 2;

std::string digest(llvm::StringRef bytes);
std::string serialize(const llvm::json::Value &value);
std::expected<std::string, std::string> readDigest(const fs::path &path);
std::expected<fs::path, std::string> identity(const fs::path &path);
fs::path resolve(const fs::path &path, const fs::path &directory);
std::expected<llvm::json::Object, std::string> fileRecord(const fs::path &path);
} // namespace facts::astcache::detail
