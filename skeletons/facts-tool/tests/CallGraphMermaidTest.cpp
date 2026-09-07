#include "analysis/callgraph/CallGraphMermaid.h"
#include <cassert>

int main() {
  using namespace facts;
  using namespace facts::callgraph;
  QueryNode root{{0xffffffffU, 42}, "operator\"<>&|\\\nend", "root-usr", true};
  QueryNode child{{7, 5}, "leaf", "leaf-usr", true};
  QueryEdge edge{root.id, child.id, RelationKind::Calls, 7, 4, 5, 30};
  QueryGraph graph{{root, child}, {edge}};
  RenderedGraph traversal;
  traversal.nodes = {root.id, child.id};
  traversal.edges = {{edge}, {edge}};
  const auto text =
      renderCallGraphMermaid(graph, traversal, "{}\n", "pair-a", nullptr,
                             EdgeView::Semantic, true, false);
  assert(text.find("#34;#60;#62;#38;#124;#92;#10;") != std::string::npos);
  assert(text.find("ffffff") != std::string::npos);
  assert(text.find("Initial partial graph; recovery pending") !=
         std::string::npos);
  const auto firstEdge = text.find(" -->");
  assert(firstEdge != std::string::npos &&
         text.find(" -->", firstEdge + 1) == std::string::npos);
  assert(text.find("offset=30") != std::string::npos);
  const auto other =
      renderCallGraphMermaid(graph, traversal, "{}\n", "pair-b", nullptr,
                             EdgeView::Semantic, true, false);
  assert(text != other);
}
