#include "analysis/callgraph/DispatchResolver.h"

#include "analysis/callgraph/OverrideIndex.h"
#include "analysis/callgraph/ReceiverPropagation.h"

#include <algorithm>
#include <clang/AST/DeclCXX.h>

namespace facts::callgraph {
namespace {

void emitDispatches(const CallFact &call,
                    const ReceiverFunctionContext &context,
                    const OverrideIndex &index, std::vector<CallFact> &output) {
  const auto *method = llvm::dyn_cast<clang::CXXMethodDecl>(call.callee);
  if (!call.virtualCall || !method || !context.receiver)
    return;
  for (const auto &target :
       index.targets(*method, *context.receiver, context.certainty,
                     call.relation.destination)) {
    const Relation relation{.source = call.relation.source,
                            .destination = target.symbol,
                            .kind = RelationKind::DispatchCalls};
    auto site = call.site;
    site.destination = relation.destination;
    site.kind = RelationKind::DispatchCalls;
    site.receiverType = context.certainty == ReceiverCertainty::Exact
                            ? context.receiverId
                            : std::nullopt;
    site.certainty = context.certainty;
    output.push_back(
        {relation, site, call.caller, target.method, context.receiver, false});
  }
}

} // namespace

std::vector<CallFact>
resolveDispatchCalls(std::span<const CallFact> calls,
                     std::span<const OverrideFact> overrides) {
  const OverrideIndex index{overrides};
  auto work = initialReceiverContexts(calls);
  std::vector<ReceiverFunctionContext> visited;
  std::vector<CallFact> output;
  for (const auto &call : calls)
    if (call.receiver && call.site.certainty && !isImplicitSelfReceiver(call))
      emitDispatches(call,
                     {call.caller, call.receiver, call.site.receiverType,
                      *call.site.certainty},
                     index, output);
  while (!work.empty()) {
    auto context = work.back();
    work.pop_back();
    if (std::ranges::any_of(visited, [&](const auto &seen) {
          return sameContext(seen, context);
        }))
      continue;
    visited.push_back(context);
    for (const auto &call : calls) {
      if (call.caller->getCanonicalDecl() !=
          context.function->getCanonicalDecl())
        continue;
      emitDispatches(call, context, index, output);
      if (!call.virtualCall && llvm::isa<clang::CXXMethodDecl>(call.callee))
        work.push_back({call.callee, context.receiver, context.receiverId,
                        context.certainty});
    }
  }
  return output;
}

} // namespace facts::callgraph
