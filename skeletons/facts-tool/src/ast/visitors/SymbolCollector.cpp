#include "ast/visitors/SymbolCollector.h"
#include "ast/StoreExtracted.h"

#include "ast/extractors/File.h"
#include "ast/extractors/FunctionDecl.h"
#include "ast/extractors/NamedDecl.h"
#include "ast/extractors/RecordDecl.h"
#include "model/AnySymbol.h"
#include "storage/FactStore.h"
#include "storage/FileManager.h"

#include <clang/AST/ASTContext.h>
#include <clang/AST/DeclCXX.h>
#include <clang/Basic/SourceManager.h>
#include <llvm/Support/Casting.h>
#include <llvm/Support/raw_ostream.h>

#include <utility>

namespace facts {
namespace {

bool hasSpecializedExtractor(const clang::NamedDecl &decl) {
  return llvm::isa<clang::CXXRecordDecl>(decl) ||
         llvm::isa<clang::FunctionDecl>(decl) ||
         llvm::isa<clang::ParmVarDecl>(decl);
}

} // namespace

void collectSymbol(clang::FunctionDecl &node, clang::ASTContext &context,
                   FileManager &files, FactStore &store) {
  storeExtracted(node, extractFunction(node, context.getSourceManager(), store),
                 context, files, store);
}

void collectSymbol(clang::NamedDecl &node, clang::ASTContext &context,
                   FileManager &files, FactStore &store) {
  if (hasSpecializedExtractor(node)) {
    return;
  }

  storeExtracted(node,
                 extractSymbol<Symbol>(node, context.getSourceManager())
                     .transform([](Symbol symbol) {
                       return classifySymbol(std::move(symbol));
                     }),
                 context, files, store);
}

} // namespace facts
