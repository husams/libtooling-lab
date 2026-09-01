#pragma once

#include "analysis/callgraph/CallGraphQuery.h"

#include <compare>

namespace facts::callgraph {

struct QueryContext {
  SymbolId node;
  std::optional<std::string> receiver;
  std::optional<ReceiverCertainty> certainty;

  auto operator<=>(const QueryContext &) const = default;
};

bool matchesContext(const QueryEdge &edge, const QueryContext &context);
QueryContext descendContext(const QueryEdge &edge, const QueryContext &context);

} // namespace facts::callgraph
