#pragma once

#include "tooling/astcache/Metadata.h"
#include <llvm/Support/JSON.h>

namespace facts::astcache::detail {
std::expected<llvm::json::Object, std::string>
lookupCandidate(const std::filesystem::path &path);
bool lookupMatches(const llvm::json::Value &value);
std::expected<llvm::json::Array, std::string>
lookupRecords(const Entry &entry, clang::ASTUnit &unit,
              const llvm::json::Array &inputs);
} // namespace facts::astcache::detail
