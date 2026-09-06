#include "ast/extractors/ExternalTarget.h"
#include "ast/extractors/NamedDecl.h"
#include "storage/SemanticProperties.h"
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/Support/JSON.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/raw_ostream.h>

namespace {
class Calls : public clang::RecursiveASTVisitor<Calls> {
public:
  Calls(const clang::FunctionDecl &caller, llvm::json::Array &rows)
      : caller_(caller), rows_(rows) {}

  bool VisitCallExpr(clang::CallExpr *call) {
    const auto *target = call->getDirectCallee();
    if (!target)
      return true;
    const auto &sm = caller_.getASTContext().getSourceManager();
    const auto &visible = facts::visibleTarget(*target, sm);
    auto usr = facts::extractUsr(*target);
    const auto site = sm.getExpansionLoc(call->getExprLoc());
    llvm::json::Array redecls;
    for (const auto *decl : target->redecls()) {
      auto location = sm.getExpansionLoc(decl->getLocation());
      redecls.push_back(llvm::json::Object{
          {"implicit", decl->isImplicit()},
          {"usable", location.isValid()},
          {"file", location.isValid() ? sm.getFilename(location).str() : ""}});
    }
    rows_.push_back(llvm::json::Object{
        {"caller", caller_.getNameAsString()},
        {"target", target->getQualifiedNameAsString()},
        {"usr", usr ? *usr : ""},
        {"implicit", target->isImplicit()},
        {"compiler_provided", facts::compilerProvided(visible, sm)},
        {"kind", facts::storage::storedSymbolKind(
                     clang::index::getSymbolInfo(target).Kind)},
        {"line", sm.getExpansionLineNumber(site)},
        {"column", sm.getExpansionColumnNumber(site)},
        {"redeclarations", std::move(redecls)}});
    return true;
  }

private:
  const clang::FunctionDecl &caller_;
  llvm::json::Array &rows_;
};

class Functions : public clang::RecursiveASTVisitor<Functions> {
public:
  llvm::json::Array rows;

  bool VisitFunctionDecl(clang::FunctionDecl *decl) {
    if (decl->getNameAsString().starts_with("case_") && decl->hasBody())
      Calls(*decl, rows).TraverseStmt(decl->getBody());
    return true;
  }
};
} // namespace

int main(int argc, char **argv) {
  if (argc != 3)
    return 2;
  auto source = llvm::MemoryBuffer::getFile(argv[1]);
  if (!source)
    return 2;
  auto ast = clang::tooling::buildASTFromCodeWithArgs(
      (*source)->getBuffer(), {"-std=c++23", "-fsized-deallocation"}, argv[1],
      argv[2]);
  if (!ast || ast->getDiagnostics().hasErrorOccurred())
    return 1;
  Functions visitor;
  visitor.TraverseDecl(ast->getASTContext().getTranslationUnitDecl());
  llvm::outs() << llvm::formatv("{0:2}\n",
                                llvm::json::Value(std::move(visitor.rows)));
}
