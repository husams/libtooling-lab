#include "analysis/callgraph/CallGraphQuery.h"
#include "analysis/callgraph/CallGraphTraversal.h"

#include <iostream>
#include <string>

namespace {

bool require(bool condition, std::string_view message) {
  if (!condition)
    std::cerr << message << '\n';
  return condition;
}

facts::callgraph::QueryGraph graph() {
  using namespace facts;
  using namespace facts::callgraph;
  const SymbolId a{1, 1}, b{1, 2}, external{1, 3}, leaf{1, 4};
  return {{QueryNode{a, "a", "usr:a", true, false},
           QueryNode{b, "b", "usr:b", true, false},
           QueryNode{external, "external", "usr:external", false, true},
           QueryNode{leaf, "leaf", "usr:leaf", true, false}},
          {QueryEdge{a, b, RelationKind::Calls, 1, 3, 4, 5},
           QueryEdge{b, a, RelationKind::Calls, 1, 7, 8, 9},
           QueryEdge{b, external, RelationKind::Calls, 1, 10, 11, 12}}};
}

facts::callgraph::QueryGraph contextualGraph() {
  using namespace facts;
  using namespace facts::callgraph;
  const SymbolId entry{2, 1}, base{2, 2}, x{2, 3}, y{2, 4};
  return {{QueryNode{entry, "entry", "usr:entry", true, false},
           QueryNode{base, "base", "usr:base", true, false},
           QueryNode{x, "x", "usr:x", true, false},
           QueryNode{y, "y", "usr:y", true, false}},
          {QueryEdge{entry, base, RelationKind::Calls, 1, 1, 1, 1, "X",
                     ReceiverCertainty::Exact},
           QueryEdge{base, x, RelationKind::DispatchCalls, 1, 2, 1, 2, "X",
                     ReceiverCertainty::Exact},
           QueryEdge{base, y, RelationKind::DispatchCalls, 1, 2, 1, 2, "Y",
                     ReceiverCertainty::Exact}}};
}

} // namespace

int main() {
  auto value = graph();
  auto byName = facts::callgraph::selectRoots(value, "a", false);
  auto byUsr = facts::callgraph::selectRoots(value, "usr:a", false);
  auto all = facts::callgraph::selectRoots(value, std::nullopt, true);
  if (!require(byName && byUsr && byName->front() == byUsr->front(),
               "name and USR selectors disagree") ||
      !require(all && all->size() == 3, "all roots omit call-free definitions"))
    return 1;
  const auto complete = facts::callgraph::renderCallGraph(value, *byName, {});
  if (!require(complete.text.find("cycle=true") != std::string::npos,
               "recursive cycle was not reported") ||
      !require(complete.text.find("external-boundary=true") !=
                   std::string::npos,
               "external boundary was not reported") ||
      !require(complete.truncated == 0, "default traversal was truncated"))
    return 1;
  const auto bounded = facts::callgraph::renderCallGraph(value, *byName, 1);
  auto contextual = contextualGraph();
  auto contextualRoot =
      facts::callgraph::selectRoots(contextual, "entry", false);
  const auto rendered = facts::callgraph::renderCallGraph(
      contextual, *contextualRoot, std::nullopt);
  return require(bounded.truncated == 1 &&
                     bounded.text.find("depth-truncated=true") !=
                         std::string::npos &&
                     rendered.text.find("target=x") != std::string::npos &&
                     rendered.text.find("target=y") == std::string::npos,
                 "explicit depth was not distinguished")
             ? 0
             : 1;
}
