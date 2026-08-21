#ifndef FACTS_TOOL_AST_VISITORS_SYMBOL_VISITOR_H
#define FACTS_TOOL_AST_VISITORS_SYMBOL_VISITOR_H

#include <clang/AST/RecursiveASTVisitor.h>

namespace facts {
class FactStore;
class FileManager;

class SymbolVisitor final : public clang::RecursiveASTVisitor<SymbolVisitor> {
public:
  SymbolVisitor(clang::ASTContext &context, FileManager &files,
                FactStore &store)
      : context_(context), files_(files), store_(store) {}

  bool TraverseTranslationUnitDecl(clang::TranslationUnitDecl *decl);
  bool TraverseParmVarDecl(clang::ParmVarDecl *decl);
  bool VisitCXXRecordDecl(clang::CXXRecordDecl *decl);
  bool VisitFunctionDecl(clang::FunctionDecl *decl);
  bool VisitNamedDecl(clang::NamedDecl *decl);

private:
  clang::ASTContext &context_;
  FileManager &files_;
  FactStore &store_;
};

} // namespace facts

#endif // FACTS_TOOL_AST_VISITORS_SYMBOL_VISITOR_H
