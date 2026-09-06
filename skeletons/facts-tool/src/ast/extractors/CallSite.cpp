#include "ast/extractors/CallSite.h"

#include "ast/extractors/CallableSite.h"
#include "ast/extractors/File.h"
#include "ast/extractors/Location.h"
#include "ast/extractors/ReceiverContext.h"

#include <clang/AST/Expr.h>

namespace facts {

ExtractionResult<std::optional<callgraph::CallFact>>
extractCallSite(const clang::FunctionDecl &caller,
                const clang::FunctionDecl &callee, const clang::Expr &site,
                const clang::SourceManager &sourceManager, FileManager &files,
                FactStore &store) {
  if (!extractLocation(sourceManager, site.getExprLoc()) ||
      !resolveFile(sourceManager, site.getExprLoc(), files))
    return std::nullopt;
  return extractReceiverContext(site, sourceManager, files, store)
      .and_then([&](ReceiverContext receiver) {
        return extractCallableSite(caller, callee, site.getExprLoc(), receiver,
                                   false, sourceManager, files, store);
      });
}

} // namespace facts
