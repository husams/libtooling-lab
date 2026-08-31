#pragma once

#include "ast/Indexing.h"

namespace clang {
class ASTContext;
}

namespace facts {
class FactStore;
class FileManager;

class CallGraphVisitor final {
public:
  CallGraphVisitor(clang::ASTContext &context, FileManager &files,
                   FactStore &store)
      : context_(context), files_(files), store_(store) {}

  IndexingResult run();

private:
  clang::ASTContext &context_;
  FileManager &files_;
  FactStore &store_;
};

} // namespace facts
