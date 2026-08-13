#ifndef FACTS_TOOL_AST_VISITORS_SYMBOLCOLLECTOR_H
#define FACTS_TOOL_AST_VISITORS_SYMBOLCOLLECTOR_H

namespace clang {
class ASTContext;
class CXXRecordDecl;
class FunctionDecl;
class NamedDecl;
} // namespace clang

namespace facts {
class FactStore;
class FileManager;

void collectSymbol(clang::FunctionDecl &node, clang::ASTContext &context,
                   FileManager &files, FactStore &store);
void collectSymbol(clang::CXXRecordDecl &node, clang::ASTContext &context,
                   FileManager &files, FactStore &store, bool isDefinition);
void collectSymbol(clang::NamedDecl &node, clang::ASTContext &context,
                   FileManager &files, FactStore &store);

} // namespace facts

#endif
