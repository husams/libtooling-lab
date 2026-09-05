#include "ast/extractors/RelationTarget.h"

#include "ast/extractors/NamedDecl.h"
#include "ast/extractors/TargetResolution.h"
#include "cli/Trace.h"
#include "storage/FactStore.h"

#include <clang/AST/Decl.h>
#include <clang/Basic/SourceManager.h>

namespace facts {

ExtractionResult<std::optional<SymbolId>>
resolveRelationTarget(const clang::NamedDecl &target,
                      const clang::SourceManager &sourceManager,
                      FileManager &files, FactStore &store) {
  auto usr = extractUsr(target);
  if (!usr) {
    cli::logVerbose(store.verbosity(), 3,
                    "facts-tool: trace: relation target name='{}' "
                    "result=filtered reason='invalid USR'",
                    target.getQualifiedNameAsString());
    return std::optional<SymbolId>{};
  }
  return store.findId(*usr)
      .transform_error(
          [](std::error_code) { return ExtractionError::RelationTarget; })
      .and_then([&](std::optional<SymbolId> id)
                    -> ExtractionResult<std::optional<SymbolId>> {
        if (id) {
          return id;
        }
        return findOrStoreSymbolTarget(target, sourceManager, files, store,
                                       *usr)
            .transform(
                [](SymbolId stored) { return std::optional<SymbolId>{stored}; })
            .transform_error([](std::error_code) {
              return ExtractionError::RelationTarget;
            });
      });
}

} // namespace facts
