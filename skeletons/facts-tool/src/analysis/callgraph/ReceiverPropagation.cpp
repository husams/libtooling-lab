#include "analysis/callgraph/ReceiverPropagation.h"

#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>

namespace facts::callgraph {

bool isImplicitSelfReceiver(const CallFact &call) {
  const auto *method = llvm::dyn_cast<clang::CXXMethodDecl>(call.caller);
  return method && call.receiver &&
         method->getParent()->getCanonicalDecl() ==
             call.receiver->getCanonicalDecl();
}

std::vector<ReceiverFunctionContext>
initialReceiverContexts(std::span<const CallFact> calls) {
  std::vector<ReceiverFunctionContext> contexts;
  for (const auto &call : calls) {
    if (!call.receiver || !call.site.certainty || isImplicitSelfReceiver(call))
      continue;
    contexts.push_back({call.callee, call.receiver, call.site.receiverType,
                        *call.site.certainty});
  }
  return contexts;
}

bool sameContext(const ReceiverFunctionContext &left,
                 const ReceiverFunctionContext &right) {
  return left.function->getCanonicalDecl() ==
             right.function->getCanonicalDecl() &&
         left.receiver->getCanonicalDecl() ==
             right.receiver->getCanonicalDecl() &&
         left.receiverId == right.receiverId &&
         left.certainty == right.certainty;
}

} // namespace facts::callgraph
