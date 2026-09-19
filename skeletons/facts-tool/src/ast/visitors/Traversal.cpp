#include "ast/visitors/Traversal.h"

#include "ast/visitors/CallGraphVisitor.h"
#include "ast/visitors/SymbolVisitor.h"
#include "cli/Verbose.h"
#include "storage/FactStore.h"

#include <clang/AST/ASTContext.h>
#include <clang/Basic/SourceManager.h>

namespace facts {

void traverse(clang::ASTContext &context, FileManager &files, FactStore &store,
              IndexingStatus &status) {
  if (store.verbosity() >= 2) {
    const auto &manager = context.getSourceManager();
    const auto source = manager.getFileEntryRefForID(manager.getMainFileID());
    cli::logVerbose(store.verbosity(), 2,
                    "facts-tool: extract: extraction started source={}",
                    source ? source->getName().str() : "<unknown>");
  }
  SymbolVisitor visitor(context, files, store, status);
  if (!visitor.TraverseDecl(context.getTranslationUnitDecl())) {
    status.record(std::unexpected(
        IndexingError{"cannot traverse translation unit declarations"}));
    return;
  }
  auto bodies = visitor.flushBodies();
  if (!bodies) {
    status.record(std::move(bodies));
    return;
  }
  status.record(CallGraphVisitor(context, files, store).run());
}

} // namespace facts
