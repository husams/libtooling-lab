#include "analysis/variableflow/Engine.h"

#include <clang/Tooling/CompilationDatabase.h>

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <ranges>
#include <set>
#include <string>
#include <string_view>
#include <utility>

namespace {

using facts::variableflow::Edge;
using facts::variableflow::Graph;
using facts::variableflow::Node;
using facts::variableflow::Request;

void check(bool condition, std::string_view message) {
  if (condition)
    return;
  std::fprintf(stderr, "DeepFlowTest: %.*s\n", static_cast<int>(message.size()),
               message.data());
  std::abort();
}

struct TempSource {
  std::filesystem::path directory;
  std::filesystem::path source;

  TempSource(std::string_view stem, std::string_view text) {
    const auto stamp =
        std::chrono::steady_clock::now().time_since_epoch().count();
    directory = std::filesystem::temp_directory_path() /
                ("facts-variable-flow-deep-" + std::string(stem) + "-" +
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

const Node *findNode(const Graph &graph, std::string_view kind,
                     std::string_view name, unsigned line = 0) {
  const auto found = std::ranges::find_if(graph.nodes, [&](const Node &node) {
    return node.kind == kind && node.name == name &&
           (line == 0 || node.location.line == line);
  });
  return found == graph.nodes.end() ? nullptr : &*found;
}

bool hasNode(const Graph &graph, std::string_view kind, std::string_view name) {
  return findNode(graph, kind, name) != nullptr;
}

bool hasNodeAtDepth(const Graph &graph, std::string_view kind,
                    std::string_view name, unsigned depth) {
  return std::ranges::any_of(graph.nodes, [&](const Node &node) {
    return node.kind == kind && node.name == name && node.depth == depth;
  });
}

bool hasEdge(const Graph &graph, std::int64_t source, std::int64_t target,
             std::string_view kind, std::int64_t callsite = 0) {
  return std::ranges::any_of(graph.edges, [&](const Edge &edge) {
    return edge.source == source && edge.target == target &&
           edge.kind == kind && (callsite == 0 || edge.callsite == callsite);
  });
}

bool hasBoundary(const Graph &graph, std::string_view reason) {
  return std::ranges::any_of(graph.boundaries, [&](const auto &boundary) {
    return boundary.reason == reason;
  });
}

void unlimitedChainReachesLeaf() {
  std::string source;
  for (int index = 44; index >= 0; --index) {
    source += "int level" + std::to_string(index) + "(int p) { return ";
    source += index == 44 ? "p + 1; }\n"
                          : "level" + std::to_string(index + 1) + "(p); }\n";
  }
  source += "int root() { int value = 1; return level0(value); }\n";
  const TempSource fixture("chain", source);
  const auto graph = run(fixture, "root", "value");
  check(hasNode(graph, "call", "level0"), "first call is missing");
  check(hasNode(graph, "call", "level44"),
        "unlimited traversal stopped before the last call");
  check(hasNodeAtDepth(graph, "parameter", "p", 45),
        "last function summary entry is missing at depth 45");
  check(!hasBoundary(graph, "depth-limit"),
        "unlimited traversal unexpectedly hit depth limit");
}

void depthCapStopsExpansion() {
  std::string source;
  for (int index = 44; index >= 0; --index) {
    source += "int level" + std::to_string(index) + "(int p) { return ";
    source += index == 44 ? "p + 1; }\n"
                          : "level" + std::to_string(index + 1) + "(p); }\n";
  }
  source += "int root() { int value = 1; return level0(value); }\n";
  const TempSource fixture("cap", source);
  const auto graph = run(fixture, "root", "value", 2);
  check(hasBoundary(graph, "depth-limit"),
        "depth cap did not produce a boundary");
  check(std::ranges::all_of(graph.nodes,
                            [](const Node &node) { return node.depth <= 2; }),
        "depth cap emitted a node beyond the requested depth");
  check(std::ranges::none_of(graph.nodes,
                             [](const Node &node) {
                               return node.name == "p" && node.depth > 2;
                             }),
        "callee summary was expanded past the depth cap");
}

void nestedReferenceEffectsReachCallerRead() {
  const TempSource fixture("nested-effects", R"cpp(void inner(int &v) { v = 2; }
void middle(int &v) { inner(v); }
int root() {
  int value = 1;
  middle(value);
  return value;
}
)cpp");
  const auto graph = run(fixture, "root", "value");
  const auto *innerCall = findNode(graph, "call", "inner", 2);
  const auto *middleCall = findNode(graph, "call", "middle", 5);
  const auto *innerWrite = findNode(graph, "write", "v", 1);
  const auto *middleWrite = findNode(graph, "write", "v", 2);
  const auto *callerWrite = findNode(graph, "write", "value", 5);
  const auto *callerRead = findNode(graph, "read", "value", 6);
  check(innerCall && middleCall && innerWrite && middleWrite && callerWrite &&
            callerRead,
        "nested reference effect facts are incomplete");
  check(hasEdge(graph, innerWrite->id, middleWrite->id, "reference-effect",
                innerCall->id),
        "inner reference effect did not reach middle");
  check(hasEdge(graph, middleWrite->id, callerWrite->id, "reference-effect",
                middleCall->id),
        "middle reference effect did not reach root");
  check(hasEdge(graph, callerWrite->id, callerRead->id, "data"),
        "nested effect did not reach the later root read");
}

void pointerMemoryReadHasCallerDefinition() {
  const TempSource fixture("pointer-memory", R"cpp(void touch(int *p) {
  int old = *p;
  *p += 1;
}
int root() {
  int value = 1;
  touch(&value);
  return value;
}
  )cpp");
  const auto graph = run(fixture, "root", "value");
  const Node *parameterEntry = nullptr;
  const Node *parameterRead = nullptr;
  const Node *parameterUpdate = nullptr;
  for (const auto &node : graph.nodes) {
    if (node.name != "p" || !node.variableUsr.ends_with("#pointee"))
      continue;
    if (node.kind == "parameter-pointee" && parameterEntry == nullptr)
      parameterEntry = &node;
    if (node.kind == "read" && parameterRead == nullptr)
      parameterRead = &node;
    if (node.kind == "update" && parameterUpdate == nullptr)
      parameterUpdate = &node;
  }
  const auto *callerInit = findNode(graph, "write", "value", 6);
  const auto *call = findNode(graph, "call", "touch", 7);
  const Node *callerInput = nullptr;
  if (parameterEntry && call)
    for (const auto &edge : graph.edges)
      if (edge.kind == "argument-pointee" &&
          edge.target == parameterEntry->id && edge.callsite == call->id)
        for (const auto &node : graph.nodes)
          if (node.id == edge.source)
            callerInput = &node;
  const auto *callerWrite = findNode(graph, "write", "value", 7);
  const auto *callerRead = findNode(graph, "read", "value", 8);
  check(parameterEntry && parameterRead && parameterUpdate && callerInit &&
            call && callerInput && callerWrite && callerRead,
        "pointer parameter memory accesses are incomplete");
  check(hasEdge(graph, callerInit->id, callerInput->id, "data"),
        "caller definition does not reach pointer argument input");
  check(hasEdge(graph, callerInput->id, parameterEntry->id, "argument-pointee",
                call->id),
        "pointer argument does not bind caller memory to pointee parameter");
  check(hasEdge(graph, parameterEntry->id, parameterRead->id, "data") &&
            hasEdge(graph, parameterEntry->id, parameterUpdate->id, "data"),
        "pointee parameter definition does not reach callee memory accesses");
  check(hasEdge(graph, parameterUpdate->id, callerWrite->id, "reference-effect",
                call->id),
        "pointee update does not create caller memory effect");
  check(hasEdge(graph, callerWrite->id, callerRead->id, "data"),
        "caller memory effect does not reach the later read");
}

void selectedFunctionPointerKeepsIndirectBoundary() {
  const TempSource fixture("indirect", R"cpp(int leaf(int p) { return p + 1; }
int root() {
  using Callback = int (*)(int);
  Callback callback = leaf;
  return callback(1);
}
)cpp");
  const auto graph = run(fixture, "root", "callback");
  check(hasNode(graph, "call", "indirect"),
        "selected function pointer did not activate its call");
  check(hasBoundary(graph, "indirect"),
        "indirect function pointer call lacks an explicit boundary");
}

void nestedPointerInputsReachMemoryRead() {
  const TempSource fixture(
      "nested-pointer", R"cpp(void inner(int *p) { int old = *p; *p = old + 1; }
void middle(int *q) { inner(q); }
int root() { int value = 1; middle(&value); return value; }
)cpp");
  const auto graph = run(fixture, "root", "value");
  const auto *initial = findNode(graph, "write", "value", 3);
  const Node *loaded = nullptr;
  for (const auto &node : graph.nodes)
    if (node.name == "p" && node.kind == "read" &&
        node.variableUsr.ends_with("#pointee"))
      loaded = &node;
  check(initial && loaded, "nested pointer input endpoints are missing");
  std::set<std::int64_t> reached{initial->id};
  bool changed = true;
  while (changed) {
    changed = false;
    for (const auto &edge : graph.edges)
      if ((edge.kind == "data" || edge.kind == "argument-pointee") &&
          reached.contains(edge.source))
        changed |= reached.insert(edge.target).second;
  }
  check(reached.contains(loaded->id),
        "caller value does not cross nested pointee input bindings");
}

} // namespace

int main() {
  unlimitedChainReachesLeaf();
  depthCapStopsExpansion();
  nestedReferenceEffectsReachCallerRead();
  pointerMemoryReadHasCallerDefinition();
  selectedFunctionPointerKeepsIndirectBoundary();
  nestedPointerInputsReachMemoryRead();
}
