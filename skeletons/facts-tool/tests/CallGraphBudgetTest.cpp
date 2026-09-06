#include "analysis/callgraph/CallGraphScope.h"
#include "analysis/callgraph/CallGraphTraversal.h"
#include <iostream>
namespace {
using namespace facts;
using namespace facts::callgraph;
bool require(bool value, std::string_view message) {
  if (!value)
    std::cerr << message << '\n';
  return value;
}
QueryGraph chain() {
  return {{QueryNode{{1, 1}, "a", "usr:a", true},
           QueryNode{{2, 1}, "b", "usr:b", true},
           QueryNode{{3, 1}, "c", "usr:c", true}},
          {QueryEdge{{1, 1}, {2, 1}, RelationKind::Calls, 1},
           QueryEdge{{2, 1}, {3, 1}, RelationKind::Calls, 2},
           QueryEdge{{3, 1}, {1, 1}, RelationKind::Calls, 3}}};
}
CoverageReport coverage() {
  return {{{1, "A", "/a", "repo"},
           {2, "B", "/b", "external"},
           {3, "C", "/c", "repo"}},
          {{.id = 1,
            .path = "/a/a.cpp",
            .componentId = 1,
            .componentName = "A",
            .componentPath = "/a",
            .componentKind = "repo",
            .projectLocal = true},
           {.id = 2,
            .path = "/b/b.cpp",
            .componentId = 2,
            .componentName = "B",
            .componentPath = "/b",
            .componentKind = "external"},
           {.id = 3,
            .path = "/c/c.cpp",
            .componentId = 3,
            .componentName = "C",
            .componentPath = "/c",
            .componentKind = "repo",
            .projectLocal = true}}};
}
RenderedGraph run(TraversalRequest request,
                  const CoverageReport *report = nullptr) {
  static const auto graph = chain();
  const auto roots = selectRoots(graph, "a", false);
  return renderCallGraph(graph, *roots, std::move(request), report);
}
} // namespace

int main() {
  TraversalRequest nodes;
  nodes.limits.nodes = 2;
  const auto nodeBound = run(nodes);
  TraversalRequest edges;
  edges.limits.edges = 1;
  const auto edgeBound = run(edges);
  TraversalRequest depth;
  depth.limits.depth = 1;
  const auto depthBound = run(depth);
  TraversalRequest timed;
  timed.limits.time = std::chrono::milliseconds(0);
  const auto timeBound = run(timed);
  TraversalRequest cancelled;
  cancelled.cancelled = [] { return true; };
  const auto cancelledResult = run(cancelled);
  auto report = coverage();
  auto selected = resolveScope({"A"}, CallsScope::all, report);
  report.components.push_back({4, "A", "/other-a", "repo"});
  const auto ambiguous = resolveScope({"A"}, CallsScope::all, report);
  TraversalRequest filtered;
  filtered.scope = *selected;
  const auto scoped = run(filtered, &report);
  return require(nodeBound.reason == "max_nodes" &&
                     nodeBound.nodes.size() == 2 && nodeBound.edges.size() == 1,
                 "node budget admitted too much") &&
                 require(edgeBound.reason == "max_edges" &&
                             edgeBound.nodes.size() == 2 &&
                             edgeBound.edges.size() == 1,
                         "edge budget admitted too much") &&
                 require(depthBound.reason == "max_depth" &&
                             depthBound.frontier.front().id == SymbolId{2, 1},
                         "depth frontier is inaccurate") &&
                 require(timeBound.reason == "time_limit",
                         "time limit was ignored") &&
                 require(cancelledResult.reason == "cancelled",
                         "cancel was ignored") &&
                 require(scoped.reason.empty() && scoped.excluded.size() == 1 &&
                             scoped.excluded.front().target == SymbolId{2, 1},
                         "component boundary was traversed") &&
                 require(!ambiguous &&
                             ambiguous.error().find("ambiguous component") !=
                                 std::string::npos,
                         "ambiguous component was selected")
             ? 0
             : 1;
}
