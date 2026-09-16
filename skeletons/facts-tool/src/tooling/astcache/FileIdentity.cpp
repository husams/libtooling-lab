#include "tooling/astcache/FileIdentity.h"
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/SHA256.h>
#include <llvm/Support/raw_ostream.h>

namespace facts::astcache::detail {
std::string digest(llvm::StringRef bytes) {
  llvm::SHA256 hash;
  hash.update(bytes);
  constexpr char digits[] = "0123456789abcdef";
  std::string result;
  for (const auto byte : hash.final()) {
    result += digits[byte >> 4U];
    result += digits[byte & 15U];
  }
  return result;
}

std::string serialize(const llvm::json::Value &value) {
  std::string text;
  llvm::raw_string_ostream(text) << value;
  return text;
}

std::expected<std::string, std::string> readDigest(const fs::path &path) {
  auto buffer = llvm::MemoryBuffer::getFile(path.string());
  if (!buffer)
    return std::unexpected(path.string() + ": " + buffer.getError().message());
  return digest((*buffer)->getBuffer());
}

std::expected<fs::path, std::string> identity(const fs::path &path) {
  std::error_code error;
  auto absolute = fs::absolute(path.empty() ? fs::path(".") : path, error);
  if (error)
    return std::unexpected(path.string() + ": " + error.message());
  auto result = fs::weakly_canonical(absolute, error);
  if (error)
    return std::unexpected(path.string() + ": " + error.message());
  return result;
}

fs::path resolve(const fs::path &path, const fs::path &directory) {
  // Preserve symlink/.. traversal until filesystem lookup. Lexical collapse
  // can select a different file when a preceding component is a symlink.
  return path.is_absolute() ? path : directory / path;
}

std::expected<llvm::json::Object, std::string>
fileRecord(const fs::path &path) {
  return identity(path).and_then([&](const fs::path &canonical) {
    return readDigest(path).transform([&](const std::string &hash) {
      return llvm::json::Object{{"path", path.string()},
                                {"identity", canonical.string()},
                                {"digest", hash}};
    });
  });
}

} // namespace facts::astcache::detail
