#include "tooling/astcache/Metadata.h"
#include "tooling/astcache/FileIdentity.h"
#include "tooling/astcache/LookupPaths.h"

#include <algorithm>
#include <clang/Basic/SourceManager.h>
#include <clang/Frontend/ASTUnit.h>
#include <clang/Lex/PreprocessingRecord.h>
#include <clang/Lex/Preprocessor.h>
#include <clang/Serialization/ASTReader.h>
#include <llvm/ADT/SmallString.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/raw_ostream.h>

namespace facts::astcache::detail {
namespace {
bool loadedModules(clang::ASTUnit &unit) {
  const auto reader = unit.getASTReader();
  return reader && std::ranges::any_of(reader->getModuleManager(),
                                      [](const auto &module) {
                                        return module.isModule();
                                      });
}

bool volatileMacros(clang::ASTUnit &unit) {
  if (unit.getPreprocessor().SawDateOrTime())
    return true;
  auto *record = unit.getPreprocessor().getPreprocessingRecord();
  if (!record)
    return false;
  return std::any_of(record->begin(), record->end(), [](const auto *entity) {
    const auto *expansion = llvm::dyn_cast<clang::MacroExpansion>(entity);
    return expansion && expansion->isBuiltinMacro() &&
           expansion->getName()->getName() == "__TIMESTAMP__";
  });
}

std::expected<llvm::json::Array, std::string>
inputRecords(const Entry &entry, clang::ASTUnit &unit) {
  if (loadedModules(unit))
    return std::unexpected("AST caching is unavailable for TUs importing modules");
  if (volatileMacros(unit))
    return std::unexpected("AST uses time-dependent builtin macros");
  llvm::json::Array records;
  auto &manager = unit.getSourceManager();
  for (auto input = manager.fileinfo_begin(); input != manager.fileinfo_end();
       ++input) {
    const auto &cache = *input->second;
    if (!cache.OrigEntry)
      continue;
    const auto path = resolve(cache.OrigEntry->getName().str(),
                              entry.working_directory);
    auto record = fileRecord(path);
    if (!record)
      return std::unexpected(record.error());
    const auto buffer = cache.getBufferIfLoaded();
    if (buffer && record->getString("digest") != digest(buffer->getBuffer()))
      return std::unexpected("input changed during parsing: " + path.string());
    records.push_back(std::move(*record));
  }
  if (records.empty())
    return std::unexpected("AST has no physical source inputs");
  return records;
}

bool matches(const llvm::json::Value &value) {
  const auto *record = value.getAsObject();
  if (!record)
    return false;
  const auto path = record->getString("path");
  const auto canonical = record->getString("identity");
  const auto hash = record->getString("digest");
  if (!path || !canonical || !hash)
    return false;
  const auto current = identity(path->str());
  const auto currentHash = readDigest(path->str());
  return current && currentHash && current->string() == *canonical &&
         *currentHash == *hash;
}

std::expected<void, std::string>
publish(const fs::path &path, const llvm::json::Value &value) {
  int descriptor = -1;
  llvm::SmallString<256> temporary;
  if (auto error = llvm::sys::fs::createUniqueFile(
          path.string() + ".tmp-%%%%%%", descriptor, temporary))
    return std::unexpected(error.message());
  llvm::raw_fd_ostream stream(descriptor, true);
  stream << value;
  stream.close();
  auto error = stream.error();
  stream.clear_error();
  if (!error)
    error = llvm::sys::fs::rename(temporary, path.string());
  if (error) {
    llvm::sys::fs::remove(temporary);
    return std::unexpected(error.message());
  }
  return {};
}
} // namespace

bool validEntry(const Entry &entry) {
  auto buffer = llvm::MemoryBuffer::getFile(entry.metadata.string());
  if (!buffer)
    return false;
  auto parsed = llvm::json::parse((*buffer)->getBuffer());
  if (!parsed) {
    llvm::consumeError(parsed.takeError());
    return false;
  }
  const auto *object = parsed->getAsObject();
  if (!object || object->getInteger("schema") != Schema ||
      object->getString("key") != entry.ast.stem().string())
    return false;
  const auto *inputs = object->getArray("inputs");
  const auto *lookups = object->getArray("lookups");
  auto astHash = readDigest(entry.ast);
  return inputs && !inputs->empty() && lookups && astHash &&
         object->getString("ast_digest") == *astHash &&
         std::ranges::all_of(*inputs, [&](const auto &input) {
           return matches(input);
         }) &&
         std::ranges::all_of(*lookups, [&](const auto &lookup) {
           return lookupMatches(lookup);
         });
}

std::expected<void, std::string> writeMetadata(const Entry &entry,
                                             clang::ASTUnit &unit) {
  return inputRecords(entry, unit).and_then([&](llvm::json::Array inputs) {
    return lookupRecords(entry, unit, inputs)
        .and_then([&](llvm::json::Array lookups) {
          return readDigest(entry.ast).and_then([&](const std::string &astHash) {
            llvm::json::Array names;
            for (const auto &name : entry.lookup_names)
              names.push_back(name);
            return publish(entry.metadata,
                           llvm::json::Object{{"schema", Schema},
                                               {"key", entry.ast.stem().string()},
                                               {"ast_digest", astHash},
                                               {"inputs", std::move(inputs)},
                                               {"lookup_names", std::move(names)},
                                               {"lookups", std::move(lookups)}});
          });
        });
  });
}
} // namespace facts::astcache::detail
