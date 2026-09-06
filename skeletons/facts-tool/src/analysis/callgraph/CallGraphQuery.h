#pragma once

#include "model/ReceiverCertainty.h"
#include "model/Relation.h"

#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <vector>

namespace facts::callgraph {

struct QueryDefinition {
  FileId file;
  unsigned offset = 0;
  unsigned size = 0;
};

struct QueryNode {
  SymbolId id;
  std::string name;
  std::string usr;
  bool definition = false;
  bool external = false;
  unsigned line = 0;
  unsigned column = 0;
  std::optional<QueryDefinition> definitionLocation;
  std::int64_t kind = 0;
  bool implicit = false;
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
  bool implicit = false;
  std::optional<SymbolId> receiverId;
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
