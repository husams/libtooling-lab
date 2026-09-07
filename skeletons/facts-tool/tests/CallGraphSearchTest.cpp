#include "analysis/callgraph/CallGraphSearch.h"
#include "analysis/callgraph/CallGraphCoverage.h"
#include "analysis/callgraph/CallGraphSelection.h"

#include <iostream>
#include <ranges>

namespace {
bool require(bool condition, std::string_view message) {
  if (!condition)
    std::cerr << message << '\n';
  return condition;
}

facts::callgraph::QueryGraph graph() {
  using namespace facts;
  using namespace facts::callgraph;
  const SymbolId a{1, 1}, b{2, 1}, c{3, 1}, target{4, 1}, absent{5, 1};
  const SymbolId external{6, 1};
  return {{QueryNode{a, "source", "usr:a", true},
           QueryNode{b, "middle_b", "usr:b", true},
           QueryNode{c, "middle_c", "usr:c", true},
           QueryNode{target, "target", "usr:t", true},
           QueryNode{absent, "absent", "usr:z", true},
           QueryNode{external, "external", "usr:x", false, true}},
          {QueryEdge{a, c, RelationKind::Calls, 1, 4, 1, 40},
           QueryEdge{c, target, RelationKind::DispatchCalls, 3, 5, 1, 50,
                     std::nullopt, ReceiverCertainty::Possible},
           QueryEdge{a, b, RelationKind::Calls, 1, 2, 1, 20},
           QueryEdge{b, target, RelationKind::Calls, 2, 3, 1, 30},
           QueryEdge{b, a, RelationKind::Calls, 2, 6, 1, 60},
           QueryEdge{a, external, RelationKind::Calls, 1, 7, 1, 70}}};
}

facts::callgraph::CoverageReport completeCoverage() {
  using facts::callgraph::CoverageFile;
  return {
      {},
      {{CoverageFile{1, "a.cpp", 0, {}, {}, {}, true, true, {}, "now", true},
        CoverageFile{2, "b.cpp", 0, {}, {}, {}, true, true, {}, "now", true},
        CoverageFile{3, "c.cpp", 0, {}, {}, {}, true, true, {}, "now", true},
        CoverageFile{4, "t.cpp", 0, {}, {}, {}, true, true, {}, "now", true},
        CoverageFile{5, "z.cpp", 0, {}, {}, {}, true, true, {}, "now", true}}},
      {}};
}
} // namespace

int main() {
  using namespace facts::callgraph;
  const auto value = graph();
  const auto source = selectOne(value, "source", "root");
  const auto target = selectOne(value, "target", "target");
  const auto absent = selectOne(value, "absent", "target");
  if (!source || !target || !absent)
    return 1;
  const auto shortest =
      searchPaths(value, **source, **target, PathMode::Shortest, {});
  const auto all =
      searchPaths(value, **source, **target, PathMode::AllSimple, {});
  const auto self =
      searchPaths(value, **source, **source, PathMode::AllSimple, {});
  const auto capped =
      searchPaths(value, **source, **target, PathMode::AllSimple, 1);
  const auto missing =
      searchPaths(value, **source, **absent, PathMode::Shortest, {});
  auto coverage = completeCoverage();
  const auto callers = searchCallers(value, {*target}, {}, &coverage);
  const auto cycleOnly = searchCallers(value, {*source}, 1, &coverage);
  const auto possible = std::ranges::find_if(all.paths, [](const auto &path) {
    return path.nodes.size() == 3 && path.nodes[1] == facts::SymbolId{3, 1};
  });
  return require(shortest.paths.size() == 1 &&
                     shortest.paths[0].nodes ==
                         std::vector<facts::SymbolId>{{1, 1}, {2, 1}, {4, 1}},
                 "shortest path tie-break is not deterministic") &&
                 require(all.paths.size() == 2 && possible != all.paths.end() &&
                             possible->edges.back().certainty ==
                                 facts::ReceiverCertainty::Possible,
                         "all-simple paths lost a possible edge") &&
                 require(self.paths.size() == 1 && self.paths[0].edges.empty(),
                         "zero-edge self path is missing") &&
                 require(pathResult(value, capped, &coverage) == "truncated",
                         "explicit path cap was not reported") &&
                 require(pathResult(value, missing, &coverage) == "not_found",
                         "external boundary invalidated complete evidence") &&
                 require(callers.edges.size() == 5 &&
                             callers.edges.front().edge.destination ==
                                 (*target)->id,
                         "reverse traversal lost oriented caller sites") &&
                 require(cycleOnly.truncated == 0,
                         "caller cycle was reported as truncation")
             ? 0
             : 1;
}
