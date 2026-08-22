#include "ast/extractors/TargetResolution.h"

#include "ast/extractors/File.h"
#include "model/AnySymbol.h"
#include "model/Symbol.h"
#include "storage/FactStore.h"
#include "storage/FileManager.h"

#include <clang/AST/Decl.h>
#include <clang/Basic/SourceManager.h>
#include <clang/Index/IndexSymbol.h>

#include <array>
#include <optional>
#include <utility>

namespace facts {
namespace {

std::expected<FileId, std::error_code>
registerExternalFile(const clang::NamedDecl &target,
                     const clang::SourceManager &sourceManager,
                     FileManager &files) {
  const auto expansion = sourceManager.getExpansionLoc(target.getLocation());
  if (expansion.isInvalid()) {
    return std::unexpected(std::make_error_code(std::errc::invalid_argument));
  }

  return extractFilePath(sourceManager, sourceManager.getFileID(expansion))
      .and_then([&files](std::string path) {
        const std::array paths{path};
        return files.addBulk(paths).and_then(
            [&files, path = std::move(path)] { return files.getId(path); });
      });
}

Symbol externalSymbol(const clang::NamedDecl &target, std::string usr) {
  Symbol symbol{};
  static_cast<clang::index::SymbolInfo &>(symbol) =
      clang::index::getSymbolInfo(&target);
  symbol.usr = std::move(usr);
  symbol.qualifiedName = target.getQualifiedNameAsString();
  symbol.flags = bit(ExternalBit);
  return symbol;
}

} // namespace

std::expected<SymbolId, std::error_code> findOrStoreSymbolTarget(
    const clang::NamedDecl &target, const clang::SourceManager &sourceManager,
    FileManager &files, FactStore &store, const std::string &usr) {
  return store.findId(usr).and_then(
      [&](std::optional<SymbolId> destination)
          -> std::expected<SymbolId, std::error_code> {
        if (destination) {
          return *destination;
        }
        return registerExternalFile(target, sourceManager, files)
            .and_then([&](FileId file) {
              return store.save(file, externalSymbol(target, usr));
            });
      });
}

} // namespace facts
