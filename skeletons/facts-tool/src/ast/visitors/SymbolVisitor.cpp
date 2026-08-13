#include "ast/visitors/SymbolVisitor.h"

#include "ast/extractors/File.h"
#include "ast/extractors/FunctionDecl.h"
#include "ast/extractors/NamedDecl.h"
#include "model/AnySymbol.h"
#include "storage/FactStore.h"
#include "storage/FileManager.h"

#include <clang/AST/ASTContext.h>
#include <clang/Basic/SourceManager.h>
#include <llvm/Support/raw_ostream.h>

namespace facts {
namespace {

template <typename Model, typename Node>
void extractAndStore(Node &node, clang::ASTContext &context, FileManager &files,
                     FactStore &store) {
  auto symbol = extractSymbol<Model>(node, context.getSourceManager());
  if (!symbol) {
    return;
  }

  auto stored =
      resolveFile(context.getSourceManager(), node.getLocation(), files)
          .and_then([&store, &symbol](FileId file) {
            return store.save(file, std::move(*symbol));
          });
  if (!stored) {
    llvm::errs() << "facts-tool: cannot persist symbol: "
                 << stored.error().message() << '\n';
  }
}

void extractAndStore(clang::NamedDecl &node, clang::ASTContext &context,
                     FileManager &files, FactStore &store) {
  auto symbol = extractSymbol<Symbol>(node, context.getSourceManager());
  if (!symbol) {
    return;
  }

  auto concrete = classifySymbol(std::move(*symbol));
  auto stored =
      resolveFile(context.getSourceManager(), node.getLocation(), files)
          .and_then([&](FileId file) {
            return store.save(file, std::move(concrete));
          });
  if (!stored) {
    llvm::errs() << "facts-tool: cannot persist symbol: "
                 << stored.error().message() << '\n';
  }
}

bool hasSpecializedExtractor(const clang::NamedDecl &decl) {
  return llvm::isa<clang::FunctionDecl>(decl) ||
         llvm::isa<clang::ParmVarDecl>(decl);
}

} // namespace

SymbolVisitor::SymbolVisitor(clang::ASTContext &context, FileManager &files,
                             FactStore &store)
    : context_(context), files_(files), store_(store) {}

bool SymbolVisitor::TraverseParmVarDecl(clang::ParmVarDecl *) { return true; }

bool SymbolVisitor::VisitFunctionDecl(clang::FunctionDecl *decl) {
  extractAndStore<Function>(*decl, context_, files_, store_);
  return true;
}

bool SymbolVisitor::VisitNamedDecl(clang::NamedDecl *decl) {
  if (!hasSpecializedExtractor(*decl)) {
    extractAndStore(*decl, context_, files_, store_);
  }
  return true;
}

} // namespace facts
