#include "analysis/callgraph/CallGraphContext.h"

namespace facts::callgraph {

bool matchesContext(const QueryEdge &edge, const QueryContext &context) {
  if (edge.kind != RelationKind::DispatchCalls ||
      context.certainty != ReceiverCertainty::Exact)
    return true;
  return edge.certainty == ReceiverCertainty::Exact &&
         edge.receiver == context.receiver;
}

QueryContext descendContext(const QueryEdge &edge,
                            const QueryContext &context) {
  if (edge.certainty)
    return {edge.destination, edge.receiver, edge.certainty};
  return {edge.destination, context.receiver, context.certainty};
}

} // namespace facts::callgraph
