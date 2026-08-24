#ifndef FACTS_TOOL_AST_VISITORS_SYMBOLCOLLECTOR_H
#define FACTS_TOOL_AST_VISITORS_SYMBOLCOLLECTOR_H

#include "ast/Indexing.h"

namespace clang {
class ASTContext;
class CXXRecordDecl;
class FunctionDecl;
class NamedDecl;
} // namespace clang

namespace facts {
class FactStore;
class FileManager;

IndexingResult collectSymbol(clang::FunctionDecl &node,
                             clang::ASTContext &context, FileManager &files,
                             FactStore &store);
IndexingResult collectSymbol(clang::CXXRecordDecl &node,
                             clang::ASTContext &context, FileManager &files,
                             FactStore &store, bool isDefinition);
IndexingResult collectSymbol(clang::NamedDecl &node, clang::ASTContext &context,
                             FileManager &files, FactStore &store);
IndexingResult collectDeclaredSymbol(clang::NamedDecl &node,
                                     clang::ASTContext &context,
                                     FileManager &files, FactStore &store);

} // namespace facts

#endif
