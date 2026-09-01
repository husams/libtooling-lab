#pragma once

#include "analysis/callgraph/CallGraphTypes.h"

#include <optional>
#include <span>
#include <vector>

namespace facts::callgraph {

struct ReceiverFunctionContext {
  const clang::FunctionDecl *function = nullptr;
  const clang::CXXRecordDecl *receiver = nullptr;
  std::optional<SymbolId> receiverId;
  ReceiverCertainty certainty = ReceiverCertainty::Possible;
};

std::vector<ReceiverFunctionContext>
initialReceiverContexts(std::span<const CallFact> calls);
bool isImplicitSelfReceiver(const CallFact &call);
bool sameContext(const ReceiverFunctionContext &left,
                 const ReceiverFunctionContext &right);

} // namespace facts::callgraph
