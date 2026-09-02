#include "commands/match/DirectCall.h"

#include "analysis/callgraph/CallGraphLinker.h"
#include "ast/extractors/CallSite.h"
#include "commands/match/ArgumentOutput.h"
#include "commands/match/NearestCaller.h"
#include "commands/match/SymbolDispatch.h"

#include <clang/AST/ASTContext.h>
#include <clang/AST/Expr.h>

namespace facts::commands::match {

std::expected<void, std::string> persistDirectCall(const DirectCallMatch &match,
                                                   clang::ASTContext &context,
                                                   FileManager &files,
                                                   FactStore &store) {
  return nearestCaller(match.call, context)
      .and_then([&](const clang::FunctionDecl *caller) {
        return persistSymbol(*caller, context, files, store)
            .transform([caller](PersistedSymbol) { return caller; });
      })
      .and_then([&](const clang::FunctionDecl *caller) {
        return persistSymbol(match.callee, context, files, store)
            .transform([caller](PersistedSymbol) { return caller; });
      })
      .and_then([&](const clang::FunctionDecl *caller)
                    -> std::expected<void, std::string> {
        auto extracted =
            extractCallSite(*caller, match.callee, match.call,
                            context.getSourceManager(), files, store);
        if (!extracted || !*extracted)
          return std::unexpected("direct call has no persistable call site");
        callgraph::CallGraphFacts facts;
        facts.calls.push_back(std::move(**extracted));
        auto linked = callgraph::linkCallGraphFacts(std::move(facts), store);
        if (!linked)
          return std::unexpected(linked.error().message);
        printArguments(match.call, context);
        return {};
      });
}

} // namespace facts::commands::match
