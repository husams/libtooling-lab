#include "ast/visitors/Traversal.h"

#include "ast/visitors/SymbolVisitor.h"

#include <clang/AST/ASTContext.h>

namespace facts {

void traverse(clang::ASTContext &context, FileManager &files,
              FactStore &store) {
  SymbolVisitor visitor(context, files, store);
  visitor.TraverseDecl(context.getTranslationUnitDecl());
}

} // namespace facts
