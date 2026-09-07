#pragma once
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/SHA256.h>
#include <string>
#include <string_view>

namespace facts::commands {
struct FramedHash {
  llvm::SHA256 hash;

  void field(std::string_view tag, std::string_view value) {
    const auto frame = std::to_string(tag.size()) + ":" + std::string(tag) +
                       std::to_string(value.size()) + ":" + std::string(value);
    hash.update(llvm::StringRef(frame));
  }

  void number(std::string_view tag, auto value) {
    field(tag, std::to_string(value));
  }

  std::string finish() {
    constexpr char digits[] = "0123456789abcdef";
    std::string result;
    for (const auto byte : hash.final()) {
      result += digits[byte >> 4U];
      result += digits[byte & 0x0fU];
    }
    return result;
  }
};
} // namespace facts::commands
