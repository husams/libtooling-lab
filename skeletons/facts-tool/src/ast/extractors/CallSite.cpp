#include "ast/extractors/CallSite.h"

#include "ast/extractors/File.h"
#include "ast/extractors/Location.h"
#include "ast/extractors/NamedDecl.h"
#include "ast/extractors/ReceiverContext.h"
#include "ast/extractors/Reference.h"
#include "ast/extractors/RelationTarget.h"
#include "storage/FactStore.h"

#include <clang/AST/DeclCXX.h>
#include <clang/AST/ExprCXX.h>

namespace facts {

ExtractionResult<std::optional<callgraph::CallFact>>
extractCallSite(const clang::FunctionDecl &caller,
                const clang::FunctionDecl &callee, const clang::Expr &site,
                const clang::SourceManager &sourceManager, FileManager &files,
                FactStore &store) {
  // Excluded sites must not materialize targets or fail target resolution.
  const auto location = extractLocation(sourceManager, site.getExprLoc());
  if (!location)
    return std::nullopt;
  const auto file = resolveFile(sourceManager, site.getExprLoc(), files);
  if (!file)
    return std::nullopt;
  const auto &sourceDecl = referenceOwner(caller);
  const auto &targetDecl = referenceOwner(callee);
  return extractUsr(sourceDecl)
      .and_then([&](std::string sourceUsr) {
        return store.findId(sourceUsr).transform_error(
            [](std::error_code) { return ExtractionError::InvalidUsr; });
      })
      .and_then([&](std::optional<SymbolId> source)
                    -> ExtractionResult<std::optional<callgraph::CallFact>> {
        if (!source)
          return std::nullopt;
        return resolveRelationTarget(targetDecl, sourceManager, files, store)
            .and_then(
                [&](std::optional<SymbolId> destination)
                    -> ExtractionResult<std::optional<callgraph::CallFact>> {
                  if (!destination)
                    return std::nullopt;
                  return extractReceiverContext(site, sourceManager, files,
                                                store)
                      .and_then([&](ReceiverContext receiver)
                                    -> ExtractionResult<
                                        std::optional<callgraph::CallFact>> {
                        const Relation relation{.source = *source,
                                                .destination = *destination,
                                                .kind = RelationKind::Calls};
                        const auto *method =
                            llvm::dyn_cast<clang::CXXMethodDecl>(&targetDecl);
                        return store.isExternal(*destination)
                            .transform([&](bool external) {
                              return std::optional<callgraph::CallFact>{
                                  callgraph::CallFact{
                                      relation,
                                      RelationSite{
                                          .source = *source,
                                          .destination = *destination,
                                          .kind = RelationKind::Calls,
                                          .file = *file,
                                          .location = *location,
                                          .receiverType = receiver.type,
                                          .certainty = receiver.certainty},
                                      &sourceDecl,
                                      &targetDecl,
                                      receiver.declaration,
                                      method && method->isVirtual(), external}};
                            })
                            .transform_error(
                                [](std::error_code) {
                                  return ExtractionError::RelationTarget;
                                });
                      });
                });
      });
}

} // namespace facts
