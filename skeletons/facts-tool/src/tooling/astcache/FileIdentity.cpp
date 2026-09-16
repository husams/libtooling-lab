#include "tooling/astcache/FileIdentity.h"
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/SHA256.h>

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

std::expected<std::string, std::string> readDigest(const fs::path &path) {
  auto buffer = llvm::MemoryBuffer::getFile(path.string());
  if (!buffer)
    return std::unexpected(path.string() + ": " + buffer.getError().message());
  return digest((*buffer)->getBuffer());
}

fs::path resolve(const fs::path &path, const fs::path &directory) {
  // Preserve symlink/.. traversal until filesystem lookup. Lexical collapse
  // can select a different file when a preceding component is a symlink.
  return path.is_absolute() ? path : directory / path;
}


} // namespace facts::astcache::detail
