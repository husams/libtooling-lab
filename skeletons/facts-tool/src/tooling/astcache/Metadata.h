#pragma once

#include "model/AstCache.h"
#include "tooling/astcache/Options.h"

#include <expected>
#include <filesystem>
#include <optional>
#include <string>

namespace clang {
class ASTUnit;
namespace tooling {
class CompilationDatabase;
}
}

namespace facts::astcache::detail {
struct Entry {
  std::filesystem::path ast;
  std::filesystem::path database;
  std::filesystem::path source;
  std::filesystem::path working_directory;
  std::string key;
};

std::expected<Entry, std::string>
locateEntry(const clang::tooling::CompilationDatabase &database,
            const std::string &source, const Options &options,
            bool clearAdjusters = false);

std::expected<std::optional<Snapshot>, std::string>
readCurrentSnapshot(const Entry &entry);
bool validEntry(const Entry &entry);
std::expected<void, std::string> writeMetadata(const Entry &entry,
                                             const Snapshot &snapshot);
} // namespace facts::astcache::detail
