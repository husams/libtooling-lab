#ifndef FACTS_TOOL_AST_VISITORS_TRAVERSAL_H
#define FACTS_TOOL_AST_VISITORS_TRAVERSAL_H

namespace clang {
class ASTContext;
}

namespace facts {
class FactStore;
class FileManager;
class IndexingStatus;

void traverse(clang::ASTContext &context, FileManager &files, FactStore &store,
              IndexingStatus &status);

} // namespace facts

#endif // FACTS_TOOL_AST_VISITORS_TRAVERSAL_H
