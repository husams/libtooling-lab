#include "ast/extractors/TargetResolution.h"

#include "ast/extractors/ExternalTarget.h"
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
std::expected<SymbolId, std::error_code> findOrStoreSymbolTarget(
    const clang::NamedDecl &target, const clang::SourceManager &sourceManager,
    FileManager &files, FactStore &store, const std::string &usr) {
  const auto &visible = visibleTarget(target, sourceManager);
  return store.findId(usr).and_then(
      [&](std::optional<SymbolId> destination)
          -> std::expected<SymbolId, std::error_code> {
        if (destination) {
          return *destination;
        }
        const auto file =
            compilerProvided(visible, sourceManager)
                ? std::expected<FileId, std::error_code>{builtinFileId}
                : resolveFile(sourceManager, visible.getLocation(), files);
        return file.and_then([&](FileId id) {
          return externalSymbol(visible, usr).and_then([&](Symbol symbol) {
            return store.save(id, std::move(symbol));
          });
        });
      });
}

} // namespace facts
