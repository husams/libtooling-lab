#include "tooling/astcache/LookupPaths.h"
#include "tooling/astcache/FileIdentity.h"

#include <clang/Frontend/ASTUnit.h>
#include <clang/Lex/HeaderSearch.h>
#include <clang/Lex/HeaderSearchOptions.h>
#include <clang/Lex/Preprocessor.h>
#include <set>

namespace facts::astcache::detail {
namespace {
void addCandidates(std::set<fs::path> &paths, const fs::path &root,
                   const std::string &name) {
  paths.insert(resolve(name, root));
  // Framework imports use Foo/header.h but search Foo.framework/Headers.
  const auto slash = name.find('/');
  if (slash == std::string::npos || slash == 0)
    return;
  const auto framework = root / (name.substr(0, slash) + ".framework");
  paths.insert((framework / "Headers" / name.substr(slash + 1)).lexically_normal());
  paths.insert((framework / "PrivateHeaders" / name.substr(slash + 1))
                   .lexically_normal());
}
} // namespace

std::expected<llvm::json::Object, std::string>
lookupCandidate(const fs::path &path) {
  std::error_code error;
  const bool exists = fs::is_regular_file(path, error);
  if (error && error != std::errc::no_such_file_or_directory &&
      error != std::errc::not_a_directory)
    return std::unexpected(path.string() + ": " + error.message());
  // Missing candidates may traverse a regular file, where canonicalization
  // reports ENOTDIR. Their absolute requested spelling is sufficient identity.
  const auto canonical = exists ? identity(path)
                                : std::expected<fs::path, std::string>(path);
  return canonical.transform([&](const fs::path &resolved) {
    return llvm::json::Object{{"kind", "candidate"}, {"path", path.string()},
                              {"identity", resolved.string()},
                              {"exists", exists}};
  });
}

bool lookupMatches(const llvm::json::Value &value) {
  const auto *record = value.getAsObject();
  if (!record)
    return false;
  const auto path = record->getString("path");
  const auto canonical = record->getString("identity");
  if (!path || !canonical)
    return false;
  if (record->getString("kind") == "candidate") {
    const auto current = lookupCandidate(path->str());
    return current && current->getString("identity") == canonical &&
           current->getBoolean("exists") == record->getBoolean("exists");
  }
  if (record->getString("kind") != "header_map")
    return false;
  const auto current = fileRecord(path->str());
  return current && current->getString("identity") == canonical &&
         current->getString("digest") == record->getString("digest");
}

std::expected<llvm::json::Array, std::string>
lookupRecords(const Entry &entry, clang::ASTUnit &unit,
              const llvm::json::Array &inputs) {
  std::set<fs::path> roots;
  for (const auto &input : inputs)
    roots.insert(fs::path(input.getAsObject()->getString("path")->str())
                     .parent_path());
  llvm::json::Array records;
  auto &search = unit.getPreprocessor().getHeaderSearchInfo();
  for (const auto &lookup : search.search_dir_range()) {
    const auto path = resolve(lookup.getName().str(), entry.working_directory);
    if (!lookup.isHeaderMap()) {
      roots.insert(path);
      continue;
    }
    auto record = fileRecord(path);
    if (!record)
      return std::unexpected(record.error());
    (*record)["kind"] = "header_map";
    records.push_back(std::move(*record));
  }
  // Clang omits nonexistent -I directories from the resolved search list.
  for (const auto &lookup : search.getHeaderSearchOpts().UserEntries)
    roots.insert(resolve(lookup.Path, entry.working_directory));
  std::set<fs::path> candidates;
  for (const auto &root : roots)
    for (const auto &name : entry.lookup_names)
      addCandidates(candidates, root, name);
  for (const auto &candidate : candidates) {
    auto record = lookupCandidate(candidate);
    if (!record)
      return std::unexpected(record.error());
    records.push_back(std::move(*record));
  }
  return records;
}
} // namespace facts::astcache::detail
