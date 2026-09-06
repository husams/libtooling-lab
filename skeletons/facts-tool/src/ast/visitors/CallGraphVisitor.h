#pragma once

#include "ast/Indexing.h"

namespace clang {
class ASTContext;
class FunctionDecl;
} // namespace clang

namespace facts {
class FactStore;
class FileManager;

namespace callgraph {
struct CallGraphFacts;
}

class CallGraphVisitor final {
public:
  CallGraphVisitor(clang::ASTContext &context, FileManager &files,
                   FactStore &store)
      : context_(context), files_(files), store_(store) {}

  IndexingResult run();

private:
  IndexingResult collectDestructors(const clang::FunctionDecl &caller,
                                    callgraph::CallGraphFacts &facts);
  clang::ASTContext &context_;
  FileManager &files_;
  FactStore &store_;
};

} // namespace facts
