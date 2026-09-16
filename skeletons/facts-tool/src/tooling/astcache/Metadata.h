#pragma once

#include <expected>
#include <filesystem>
#include <string>
#include <vector>

namespace clang {
class ASTUnit;
namespace tooling {
class CompilationDatabase;
}
}

namespace facts::astcache::detail {
struct Entry {
  std::filesystem::path ast;
  std::filesystem::path metadata;
  std::filesystem::path source;
  std::filesystem::path working_directory;
  std::vector<std::string> lookup_names;
};

std::expected<Entry, std::string>
locateEntry(const clang::tooling::CompilationDatabase &database,
            const std::string &source, const std::filesystem::path &directory,
            bool clearAdjusters = false);

bool validEntry(const Entry &entry);
std::expected<void, std::string> writeMetadata(const Entry &entry,
                                             clang::ASTUnit &unit);
} // namespace facts::astcache::detail
