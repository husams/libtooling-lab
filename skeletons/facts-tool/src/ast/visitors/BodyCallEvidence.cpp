#include "ast/visitors/BodyVisitor.h"

#include "ast/StoreExtracted.h"
#include "ast/extractors/File.h"
#include "ast/extractors/Location.h"
#include "ast/extractors/NamedDecl.h"
#include "ast/extractors/UnsupportedSemantics.h"
#include "storage/FactStore.h"

#include <clang/AST/ASTContext.h>
#include <clang/AST/Expr.h>

namespace facts {

bool BodyVisitor::VisitCallExpr(clang::CallExpr *expression) {
  if (expression == nullptr || expression->getDirectCallee() != nullptr) {
    return true;
  }
  if (!expression->isTypeDependent() && !expression->isValueDependent())
    reportUnsupportedSemantic("indirect-call", expression->getExprLoc(),
                              context_.getSourceManager(), files_, store_);
  auto location =
      extractLocation(context_.getSourceManager(), expression->getExprLoc());
  if (!location) {
    if (!isFilteredExtraction(location.error())) {
      status_.record(std::unexpected(
          IndexingError{"cannot extract unresolved call site: " +
                        std::string{extractionErrorName(location.error())}}));
    }
    return true;
  }
  auto file = resolveFile(context_.getSourceManager(), expression->getExprLoc(),
                          files_);
  if (!file) {
    status_.record(std::unexpected(
        IndexingError{"cannot resolve unresolved call site file: " +
                      file.error().message()}));
    return true;
  }
  auto usr = extractUsr(referenceOwner(owner_));
  if (!usr) {
    if (!isFilteredExtraction(usr.error())) {
      status_.record(std::unexpected(
          IndexingError{"cannot identify unresolved call site owner: " +
                        std::string{extractionErrorName(usr.error())}}));
    }
    return true;
  }
  auto source = store_.findId(*usr);
  if (!source) {
    status_.record(std::unexpected(
        IndexingError{"cannot look up unresolved call site owner: " +
                      source.error().message()}));
    return true;
  }
  if (!*source) {
    status_.record(std::unexpected(
        IndexingError{"unresolved call site owner was not persisted"}));
    return true;
  }
  store_.stageUnresolvedCallSite(
      UnresolvedCallSite{**source, *file, *location});
  return true;
}

IndexingResult BodyVisitor::stageEvidence() {
  auto usr = extractUsr(referenceOwner(owner_));
  if (!usr)
    return {};
  return store_.findId(*usr)
      .transform([&](std::optional<SymbolId> id) {
        if (id)
          store_.stageCallGraphEntry(*id);
      })
      .transform_error([](std::error_code error) {
        return IndexingError{"cannot stage call graph entry: " +
                             error.message()};
      });
}

} // namespace facts
