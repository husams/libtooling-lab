#pragma once

#include "model/Relation.h"
#include "model/ReceiverCertainty.h"

#include <expected>
#include <optional>
#include <string>
#include <vector>

namespace facts::callgraph {

struct QueryNode {
  SymbolId id;
  std::string name;
  std::string usr;
  bool definition = false;
  bool external = false;
};

struct QueryEdge {
  SymbolId source;
  SymbolId destination;
  RelationKind kind;
  FileId file;
  unsigned line = 0;
  unsigned column = 0;
  unsigned offset = 0;
  std::optional<std::string> receiver;
  std::optional<ReceiverCertainty> certainty;
};

struct QueryGraph {
  std::vector<QueryNode> nodes;
  std::vector<QueryEdge> edges;
};

using QueryResult = std::expected<QueryGraph, std::string>;

QueryResult loadCallGraph(const std::string &path);
std::expected<std::vector<const QueryNode *>, std::string>
selectRoots(const QueryGraph &graph, const std::optional<std::string> &function,
            bool all);

} // namespace facts::callgraph
