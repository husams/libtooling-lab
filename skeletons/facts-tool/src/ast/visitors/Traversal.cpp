#include "ast/visitors/Traversal.h"

#include "ast/visitors/CallGraphVisitor.h"
#include "ast/visitors/SymbolVisitor.h"

#include <clang/AST/ASTContext.h>

namespace facts {

void traverse(clang::ASTContext &context, FileManager &files, FactStore &store,
              IndexingStatus &status) {
  SymbolVisitor visitor(context, files, store, status);
  if (!visitor.TraverseDecl(context.getTranslationUnitDecl())) {
    status.record(std::unexpected(
        IndexingError{"cannot traverse translation unit declarations"}));
    return;
  }
  auto bodies = visitor.flushBodies();
  if (!bodies) {
    status.record(std::move(bodies));
    return;
  }
  status.record(CallGraphVisitor(context, files, store).run());
}

} // namespace facts
