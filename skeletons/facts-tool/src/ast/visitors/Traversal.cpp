#include "ast/visitors/Traversal.h"

#include "ast/visitors/SymbolVisitor.h"

#include <clang/AST/ASTContext.h>

namespace facts {

void traverse(clang::ASTContext &context, FileManager &files, FactStore &store,
              IndexingStatus &status) {
  SymbolVisitor visitor(context, files, store, status);
  visitor.TraverseDecl(context.getTranslationUnitDecl());
}

} // namespace facts
