#include "analysis/variableflow/Engine.h"

#include <clang/Tooling/CompilationDatabase.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <queue>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace {

using facts::variableflow::Edge;
using facts::variableflow::Graph;
using facts::variableflow::Node;
using facts::variableflow::Request;

void check(bool condition, std::string_view message) {
  if (condition)
    return;
  std::cerr << "DependencyFlowTest: " << message << '\n';
  std::abort();
}

struct TempSource {
  std::filesystem::path directory;
  std::filesystem::path source;

  TempSource(std::string_view stem, std::string_view text) {
    const auto stamp =
        std::chrono::steady_clock::now().time_since_epoch().count();
    directory = std::filesystem::temp_directory_path() /
                ("facts-variable-flow-dependency-" + std::string(stem) + "-" +
                 std::to_string(stamp));
    std::filesystem::create_directories(directory);
    source = directory / "fixture.cpp";
    std::ofstream(source) << text;
  }

  ~TempSource() { std::filesystem::remove_all(directory); }
};

Graph run(const TempSource &fixture, std::string function, std::string variable,
          std::optional<unsigned> maxDepth = std::nullopt) {
  clang::tooling::FixedCompilationDatabase commands(fixture.directory.string(),
                                                    {"-std=c++17"});
  auto result = facts::variableflow::analyse(
      commands, {fixture.source.string()},
      Request{std::move(function), std::move(variable), std::nullopt,
              maxDepth});
  check(result.has_value(), result ? "" : result.error());
  return std::move(*result);
}

std::vector<const Node *> nodes(const Graph &graph, std::string_view kind,
                                std::string_view name = {}) {
  std::vector<const Node *> result;
  for (const auto &node : graph.nodes)
    if (node.kind == kind && (name.empty() || node.name == name))
      result.push_back(&node);
  return result;
}

const Node &one(const Graph &graph, std::string_view kind,
                std::string_view name) {
  const auto matches = nodes(graph, kind, name);
  check(matches.size() == 1, "expected exactly one matching node");
  return *matches.front();
}

bool hasEdge(const Graph &graph, std::int64_t source, std::int64_t target,
             std::string_view kind,
             std::optional<std::int64_t> callsite = std::nullopt) {
  return std::ranges::any_of(graph.edges, [&](const Edge &edge) {
    return edge.source == source && edge.target == target &&
           edge.kind == kind && (!callsite || edge.callsite == callsite);
  });
}

bool hasArgumentToCall(const Graph &graph, std::int64_t source,
                       std::int64_t call) {
  return std::ranges::any_of(graph.edges, [&](const Edge &edge) {
    return edge.source == source && edge.target == call &&
           edge.callsite == call &&
           (edge.kind == "argument" || edge.kind == "argument-copy" ||
            edge.kind == "argument-ref" || edge.kind == "argument-pointer");
  });
}

void requireUniqueFacts(const Graph &graph) {
  std::set<
      std::tuple<std::string, std::string, std::string, std::string, unsigned>>
      facts;
  for (const auto &node : graph.nodes) {
    if (node.kind == "control")
      continue;
    check(facts
              .emplace(node.kind, node.functionUsr, node.variableUsr,
                       node.location.file, node.location.offset)
              .second,
          "duplicate semantic occurrence node");
  }
  std::set<std::tuple<std::int64_t, std::int64_t, std::string,
                      std::optional<std::int64_t>>>
      edges;
  for (const auto &edge : graph.edges)
    check(edges.emplace(edge.source, edge.target, edge.kind, edge.callsite)
              .second,
          "duplicate semantic edge");
}

void requireControlMembership(const Graph &graph) {
  for (const auto &node : graph.nodes) {
    if (node.kind == "control" || node.block < 0)
      continue;
    const auto control = std::find_if(
        graph.nodes.begin(), graph.nodes.end(), [&](const Node &candidate) {
          return candidate.kind == "control" &&
                 candidate.functionUsr == node.functionUsr &&
                 candidate.block == node.block;
        });
    check(control != graph.nodes.end(),
          "occurrence has no matching CFG block node");
    check(hasEdge(graph, control->id, node.id, "contains"),
          "CFG block does not contain its occurrence");
  }
}

void requireConnected(const Graph &graph) {
  check(!graph.nodes.empty(), "empty graph");
  std::set<std::int64_t> reached{graph.nodes.front().id};
  std::queue<std::int64_t> pending;
  pending.push(graph.nodes.front().id);
  while (!pending.empty()) {
    const auto current = pending.front();
    pending.pop();
    for (const auto &edge : graph.edges) {
      if (edge.source != current && edge.target != current)
        continue;
      const auto next = edge.source == current ? edge.target : edge.source;
      if (reached.insert(next).second)
        pending.push(next);
    }
  }
  check(reached.size() == graph.nodes.size(),
        "graph contains disconnected fact islands");
}

void copiesReachLeaf() {
  const TempSource fixture("copies", R"cpp(int leaf(int p) { return p + 1; }
int copies(int input) {
  int copy = input;
  copy = leaf(copy);
  return copy;
}
)cpp");
  const auto graph = run(fixture, "copies", "input");
  const auto &entry = one(graph, "parameter", "input");
  const auto inputReads = nodes(graph, "read", "input");
  const auto copyWrites = nodes(graph, "write", "copy");
  const auto copyReads = nodes(graph, "read", "copy");
  const auto &leafCall = one(graph, "call", "leaf");
  const auto &leafParameter = one(graph, "parameter", "p");
  const auto &leafRead = one(graph, "read", "p");
  check(inputReads.size() == 1 && copyWrites.size() == 2 &&
            copyReads.size() == 2,
        "copy occurrences are incomplete");
  check(hasEdge(graph, entry.id, inputReads.front()->id, "data"),
        "parameter entry does not reach its read");
  const auto initialCopy = std::ranges::find_if(
      copyWrites, [](const Node *node) { return node->location.line == 3; });
  check(initialCopy != copyWrites.end() &&
            hasEdge(graph, inputReads.front()->id, (*initialCopy)->id, "value"),
        "initializer read does not produce the copy");
  const auto callRead = std::ranges::find_if(copyReads, [&](const Node *node) {
    return node->location.line == leafCall.location.line;
  });
  check(callRead != copyReads.end(), "copy read at the leaf call is missing");
  check(hasEdge(graph, (*initialCopy)->id, (*callRead)->id, "data"),
        "copy definition does not reach the call argument");
  check(hasEdge(graph, (*callRead)->id, leafParameter.id, "argument-copy",
                leafCall.id),
        "callsite argument binding is not sourced from its read");
  check(hasEdge(graph, leafParameter.id, leafRead.id, "data"),
        "callee parameter entry does not reach its read");
  requireUniqueFacts(graph);
  requireControlMembership(graph);
  requireConnected(graph);
}

void callsitesShareOneSummaryWithoutStaleArguments() {
  const TempSource fixture("callsites", R"cpp(int id(int p) { return p; }
int twice(int seed) {
  int before = seed;
  int first = id(seed + 1);
  int second = id(seed + 2);
  return first + second;
}
)cpp");
  const auto graph = run(fixture, "twice", "seed");
  const auto &entry = one(graph, "parameter", "seed");
  const auto reads = nodes(graph, "read", "seed");
  const auto calls = nodes(graph, "call", "id");
  const auto &parameter = one(graph, "parameter", "p");
  const auto &summary = one(graph, "return", "id");
  check(reads.size() == 3 && calls.size() == 2,
        "caller reads or calls are missing");
  for (const auto *read : reads)
    check(hasEdge(graph, entry.id, read->id, "data"),
          "stable parameter entry is lost");
  for (const auto *call : calls) {
    const auto atCall = std::ranges::find_if(reads, [&](const Node *read) {
      return read->location.line == call->location.line;
    });
    check(atCall != reads.end(), "call argument read is missing");
    check(
        hasEdge(graph, (*atCall)->id, parameter.id, "argument-copy", call->id),
        "argument binding uses a stale access or wrong callsite");
    check(std::ranges::count_if(graph.edges,
                                [&](const Edge &edge) {
                                  return edge.source == summary.id &&
                                         edge.target == call->id &&
                                         edge.kind == "return" &&
                                         edge.callsite == call->id;
                                }) == 1,
          "call does not have exactly one callee return summary edge");
  }
  requireUniqueFacts(graph);
}

void returnedLocalExcludesSupplierNoise() {
  const TempSource fixture("supplier", R"cpp(int leaf(int p) { return p; }
int supplier() {
  int result = leaf(1);
  int noise = leaf(999);
  return result;
}
int root() {
  int value = supplier();
  value = supplier();
  return value;
}
)cpp");
  const auto graph = run(fixture, "root", "value");
  const auto suppliers = nodes(graph, "call", "supplier");
  const auto leaves = nodes(graph, "call", "leaf");
  const auto &summary = one(graph, "return", "supplier");
  check(suppliers.size() == 2, "both supplier callsites must remain distinct");
  check(leaves.size() == 1 && leaves.front()->location.line == 3,
        "unreturned supplier local leaked into the dependency graph");
  check(nodes(graph, "write", "noise").empty() &&
            nodes(graph, "read", "noise").empty(),
        "supplier noise occurrences leaked into the graph");
  for (const auto *call : suppliers)
    check(std::ranges::count_if(graph.edges,
                                [&](const Edge &edge) {
                                  return edge.source == summary.id &&
                                         edge.target == call->id &&
                                         edge.kind == "return" &&
                                         edge.callsite == call->id;
                                }) == 1,
          "supplier return summary is missing or duplicated at a callsite");
  requireUniqueFacts(graph);
}

void referenceEffectsBecomeCallerWrites() {
  const TempSource fixture("effects", R"cpp(void byref(int &p) { p = 2; }
void pointee(int *p) { *p = 3; }
void reassign(int *p) { p = nullptr; }
int effects() {
  int value = 1;
  byref(value);
  int after_ref = value;
  pointee(&value);
  int after_pointee = value;
  reassign(&value);
  int *local = &value;
  local = nullptr;
  return value + after_ref + after_pointee;
}
)cpp");
  const auto graph = run(fixture, "effects", "value");
  for (const auto &[name, calleeLine] :
       {std::pair{std::string_view{"byref"}, 1U},
        std::pair{std::string_view{"pointee"}, 2U}}) {
    const auto &call = one(graph, "call", name);
    const auto callerWrite =
        std::ranges::find_if(graph.nodes, [&](const Node &node) {
          return node.kind == "write" && node.name == "value" &&
                 node.location.line == call.location.line;
        });
    check(callerWrite != graph.nodes.end(),
          "callee effect has no caller-side write");
    const auto calleeWrite =
        std::ranges::find_if(graph.nodes, [&](const Node &node) {
          return node.kind == "write" && node.name == "p" &&
                 node.location.line == calleeLine;
        });
    check(calleeWrite != graph.nodes.end(), "callee effect write is missing");
    check(hasEdge(graph, calleeWrite->id, callerWrite->id, "reference-effect",
                  call.id),
          "callee effect is not connected to the caller write");
    const auto followingRead =
        std::ranges::find_if(graph.nodes, [&](const Node &node) {
          return node.kind == "read" && node.name == "value" &&
                 node.location.line == call.location.line + 1;
        });
    check(followingRead != graph.nodes.end() &&
              hasEdge(graph, callerWrite->id, followingRead->id, "data"),
          "caller-side effect does not reach the following read");
  }
  const auto &reassign = one(graph, "call", "reassign");
  check(std::ranges::none_of(graph.nodes,
                             [&](const Node &node) {
                               return node.kind == "write" &&
                                      node.name == "value" &&
                                      (node.location.line ==
                                           reassign.location.line ||
                                       node.location.line == 12);
                             }),
        "pointer reassignment was misclassified as a pointee write");
}

void boundariesPreserveCallerFlow() {
  const TempSource fixture("boundaries", R"cpp(int child(int p) { return p; }
int external(int);
int limited(int value) { value = child(value); return value; }
int opaque(int value) { value = external(value); return value; }
)cpp");
  for (const auto &[function, callee, depth] : std::vector<
           std::tuple<std::string, std::string, std::optional<unsigned>>>{
           {"limited", "child", 0U}, {"opaque", "external", std::nullopt}}) {
    const auto graph = run(fixture, function, "value", depth);
    const auto &call = one(graph, "call", callee);
    const auto callRead =
        std::ranges::find_if(graph.nodes, [&](const Node &node) {
          return node.kind == "read" && node.name == "value" &&
                 node.location.line == call.location.line;
        });
    const auto resultWrite =
        std::ranges::find_if(graph.nodes, [&](const Node &node) {
          return node.kind == "write" && node.name == "value" &&
                 node.location.line == call.location.line;
        });
    check(callRead != graph.nodes.end() &&
              hasArgumentToCall(graph, callRead->id, call.id),
          "boundary discarded caller read-to-call flow");
    check(resultWrite != graph.nodes.end() &&
              hasEdge(graph, call.id, resultWrite->id, "call-result", call.id),
          "boundary discarded call-to-result flow");
    check(std::ranges::any_of(graph.boundaries,
                              [&](const auto &boundary) {
                                return boundary.node == call.id &&
                                       boundary.reason == (callee == "child"
                                                               ? "depth-limit"
                                                               : "external");
                              }),
          "expected call boundary is missing");
  }
}

void recursionConvergesWithStableSummaries() {
  const TempSource fixture(
      "recursion",
      R"cpp(int recursive(int p) { return p ? recursive(p - 1) : p; }
int start(int value) { return recursive(value); }
)cpp");
  const auto graph = run(fixture, "start", "value");
  const auto calls = nodes(graph, "call", "recursive");
  const auto &parameter = one(graph, "parameter", "p");
  const auto &summary = one(graph, "return", "recursive");
  check(calls.size() == 2 && graph.nodes.size() < 100,
        "recursive traversal did not converge to two callsites");
  for (const auto *call : calls)
    check(std::ranges::count_if(graph.edges,
                                [&](const Edge &edge) {
                                  return edge.source == summary.id &&
                                         edge.target == call->id &&
                                         edge.kind == "return" &&
                                         edge.callsite == call->id;
                                }) == 1,
          "recursive return summary is not bound once per callsite");
  check(std::ranges::count_if(graph.nodes,
                              [&](const Node &node) {
                                return node.kind == "parameter" &&
                                       node.id == parameter.id;
                              }) == 1,
        "recursive parameter entry is unstable");
  requireUniqueFacts(graph);
}

void knownPointerArgumentsWriteTheirPointee() {
  const TempSource fixture("known-pointer", R"cpp(void set(int *p) { *p = 1; }
int root() {
  int value = 0;
  int *pointer = &value;
  set(pointer);
  return value;
}
)cpp");
  const auto graph = run(fixture, "root", "value");
  const auto &call = one(graph, "call", "set");
  const auto callerWrite =
      std::ranges::find_if(graph.nodes, [&](const Node &node) {
        return node.kind == "write" && node.name == "value" &&
               node.location.line == call.location.line;
      });
  const auto calleeWrite =
      std::ranges::find_if(graph.nodes, [](const Node &node) {
        return node.kind == "write" && node.name == "p" &&
               node.location.line == 1;
      });
  const auto laterRead =
      std::ranges::find_if(graph.nodes, [](const Node &node) {
        return node.kind == "read" && node.name == "value" &&
               node.location.line == 6;
      });
  check(callerWrite != graph.nodes.end() && calleeWrite != graph.nodes.end(),
        "known pointer effect did not resolve to caller storage");
  check(hasEdge(graph, calleeWrite->id, callerWrite->id, "reference-effect",
                call.id),
        "known pointee effect is not linked to its caller write");
  check(laterRead != graph.nodes.end() &&
            hasEdge(graph, callerWrite->id, laterRead->id, "data"),
        "known pointee write does not reach the later caller read");
  check(std::ranges::none_of(graph.nodes,
                             [&](const Node &node) {
                               return node.kind == "write" &&
                                      node.name == "pointer" &&
                                      node.location.line == call.location.line;
                             }),
        "known pointee effect was attached to the pointer object");
}

void unsupportedAssignmentsUseOneOccurrenceIdentity() {
  const TempSource fixture("unsupported-identity",
                           R"cpp(struct Record { int field; };
int root() {
  Record value{};
  value.field = 1;
  return 0;
}
)cpp");
  const auto graph = run(fixture, "root", "value");
  const auto unsupported =
      std::ranges::find_if(graph.nodes, [](const Node &node) {
        return node.kind == "unsupported" && node.location.line == 4;
      });
  check(unsupported != graph.nodes.end(),
        "member assignment boundary occurrence is missing");
  check(std::ranges::count_if(graph.nodes,
                              [](const Node &node) {
                                return node.kind == "unsupported" &&
                                       node.location.line == 4;
                              }) == 1,
        "one member assignment produced duplicate unsupported occurrences");
  for (const auto reason : {std::string_view{"unsupported-object"},
                            std::string_view{"unsupported-storage"}})
    check(std::ranges::any_of(graph.boundaries,
                              [&](const auto &boundary) {
                                return boundary.node == unsupported->id &&
                                       boundary.reason == reason;
                              }),
          "canonical unsupported occurrence lost a boundary reason");
  requireUniqueFacts(graph);
}

void conflictingCallArgumentsHaveAnOrderBoundary() {
  const TempSource fixture("argument-order", R"cpp(void consume(int, int) {}
int update_first(int value) { consume(value++, value); return value; }
int update_second(int value) { consume(value, value++); return value; }
)cpp");
  for (const auto function :
       {std::string{"update_first"}, std::string{"update_second"}}) {
    const auto graph = run(fixture, function, "value");
    const auto &call = one(graph, "call", "consume");
    check(std::ranges::any_of(graph.boundaries,
                              [&](const auto &boundary) {
                                return boundary.node == call.id &&
                                       boundary.reason ==
                                           "unspecified-evaluation-order";
                              }),
          "conflicting call arguments lack an evaluation-order boundary");
    check(graph.status == "partial",
          "argument-order uncertainty is reported complete");
  }
}

void multilevelPointersStopAtAnExplicitBoundary() {
  const TempSource fixture("pointer-depth",
                           R"cpp(void deep(int **p) { **p = 1; }
int root() {
  int value = 0;
  int *pointer = &value;
  deep(&pointer);
  return value;
}
)cpp");
  const auto graph = run(fixture, "root", "value");
  const auto &call = one(graph, "call", "deep");
  check(std::ranges::any_of(graph.boundaries,
                            [](const auto &boundary) {
                              return boundary.reason ==
                                     "unsupported-alias-depth";
                            }),
        "multilevel pointer access lacks an explicit alias-depth boundary");
  check(std::ranges::none_of(graph.nodes,
                             [&](const Node &node) {
                               return node.kind == "write" &&
                                      node.name == "pointer" &&
                                      node.location.line == call.location.line;
                             }),
        "unresolved multilevel pointee was misreported as a pointer-object "
        "write");
  check(graph.status == "partial",
        "unsupported pointer depth is reported complete");
}

} // namespace

int main() {
  copiesReachLeaf();
  callsitesShareOneSummaryWithoutStaleArguments();
  returnedLocalExcludesSupplierNoise();
  referenceEffectsBecomeCallerWrites();
  boundariesPreserveCallerFlow();
  recursionConvergesWithStableSummaries();
  knownPointerArgumentsWriteTheirPointee();
  unsupportedAssignmentsUseOneOccurrenceIdentity();
  conflictingCallArgumentsHaveAnOrderBoundary();
  multilevelPointersStopAtAnExplicitBoundary();
}
