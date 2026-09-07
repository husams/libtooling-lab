#include "analysis/callgraph/CallGraphScope.h"
#include "analysis/callgraph/CallGraphSearch.h"
#include <iostream>
using namespace facts;
using namespace facts::callgraph;

namespace {
bool require(bool ok, std::string_view message) {
  if (!ok)
    std::cerr << message << '\n';
  return ok;
}
} // namespace

int main() {
  QueryGraph graph{{QueryNode{{1, 1}, "a", "a", true},
                    QueryNode{{2, 1}, "b", "b", true},
                    QueryNode{{3, 1}, "c", "c", true}},
                   {QueryEdge{{1, 1}, {2, 1}, RelationKind::Calls, 1},
                    QueryEdge{{2, 1}, {3, 1}, RelationKind::Calls, 2}}};
  const std::vector<const QueryNode *> roots{&graph.nodes.back()};
  bool ok = true;
  for (bool nodes : {true, false}) {
    TraversalRequest request;
    if (nodes)
      request.limits.nodes = 2;
    else
      request.limits.edges = 1;
    const auto reason = nodes ? "max_nodes" : "max_edges";
    const auto callers = searchCallersWithRequest(graph, roots, request);
    auto paths = searchPathsWithRequest(graph, graph.nodes[0], graph.nodes[2],
                                        PathMode::AllSimple, request);
    ok &= require(callers.reason == reason && callers.nodes.size() == 2 &&
                      callers.edges.size() == 1,
                  "reverse budget ignored");
    ok &= require(paths.traversal.reason == reason && paths.paths.empty() &&
                      paths.traversal.nodes.size() == 2,
                  "path budget ignored");
    ok &= require(callers.limits.nodes == request.limits.nodes &&
                      callers.limits.edges == request.limits.edges &&
                      callers.scope.calls == CallsScope::all,
                  "reverse search lost its scope or limits");
  }
  for (bool cancelled : {true, false}) {
    TraversalRequest request;
    if (cancelled)
      request.cancelled = [] { return true; };
    else
      request.limits.time = std::chrono::milliseconds(0);
    const auto reason = cancelled ? "cancelled" : "time_limit";
    const auto callers = searchCallersWithRequest(graph, roots, request);
    const auto paths = searchPathsWithRequest(
        graph, graph.nodes[0], graph.nodes[2], PathMode::Shortest, request);
    ok &= require(callers.reason == reason && callers.nodes.empty(),
                  "reverse interrupt ignored");
    ok &= require(paths.traversal.reason == reason && paths.paths.empty(),
                  "path interrupt ignored");
  }
  CoverageReport coverage;
  coverage.components = {{1, "A", "/a", "repo"}, {2, "B", "/b", "repo"}};
  coverage.files = {{.id = 1, .componentId = 1, .projectLocal = true},
                    {.id = 2, .componentId = 2, .projectLocal = true},
                    {.id = 3, .componentId = 1, .projectLocal = true}};
  TraversalRequest scope;
  scope.scope = *resolveScope({"A"}, CallsScope::all, coverage);
  const auto callers = searchCallersWithRequest(graph, roots, scope, &coverage);
  const auto paths =
      searchPathsWithRequest(graph, graph.nodes[0], graph.nodes[2],
                             PathMode::AllSimple, scope, &coverage);
  ok &= require(callers.nodes.size() == 1 && callers.excluded.size() == 1,
                "reverse search crossed excluded endpoint");
  ok &= require(paths.paths.empty() && paths.traversal.excluded.size() == 1,
                "path search reconnected across excluded endpoint");
  ok &= require(callers.excludedNodes.size() == 1 &&
                    callers.excluded.front().source == graph.nodes[1].id &&
                    callers.excluded.front().target == graph.nodes[2].id,
                "reverse exclusion changed edge direction or node count");
  TraversalRequest depth;
  depth.limits.depth = 1;
  const auto capped = searchPathsWithRequest(
      graph, graph.nodes[0], graph.nodes[2], PathMode::Shortest, depth);
  ok &= require(capped.traversal.reason == "max_depth" &&
                    capped.traversal.frontier.size() == 1,
                "path depth truncation lost its reason or frontier");
  return ok ? 0 : 1;
}
