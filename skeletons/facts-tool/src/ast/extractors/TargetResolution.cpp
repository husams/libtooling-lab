#include "ast/extractors/TargetResolution.h"

#include "ast/extractors/File.h"
#include "model/AnySymbol.h"
#include "model/Symbol.h"
#include "storage/FactStore.h"
#include "storage/FileManager.h"

#include <clang/AST/Decl.h>
#include <clang/Basic/SourceManager.h>
#include <clang/Index/IndexSymbol.h>

#include <optional>
#include <utility>

namespace facts {
namespace {

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
  const auto &visibleTarget = *target.getMostRecentDecl();
  return store.findId(usr).and_then(
      [&](std::optional<SymbolId> destination)
          -> std::expected<SymbolId, std::error_code> {
        if (destination) {
          return *destination;
        }
        return resolveFile(sourceManager, visibleTarget.getLocation(), files)
            .and_then([&](FileId file) {
              return store.save(file, externalSymbol(visibleTarget, usr));
            });
      });
}

} // namespace facts
