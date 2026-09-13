#include "analysis/variableflow/Engine.h"

#include <clang/Tooling/CompilationDatabase.h>

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {

class TempSource {
public:
  explicit TempSource(std::string source) {
    const auto stamp =
        std::chrono::steady_clock::now().time_since_epoch().count();
    directory_ = std::filesystem::temp_directory_path() /
                 ("facts-variable-flow-cfg-" + std::to_string(stamp));
    std::filesystem::create_directories(directory_);
    path_ = directory_ / "source.cpp";
    std::ofstream{path_} << source;
  }

  ~TempSource() { std::filesystem::remove_all(directory_); }

  const std::filesystem::path &path() const { return path_; }

private:
  std::filesystem::path directory_;
  std::filesystem::path path_;
};

auto analyse(const TempSource &source) {
  clang::tooling::FixedCompilationDatabase database(".", {"-std=c++17"});
  return facts::variableflow::analyse(
      database, {source.path().string()},
      {"root", "x", std::nullopt, std::nullopt});
}

std::vector<std::int64_t> nodesAt(const facts::variableflow::Graph &graph,
                                  std::string_view kind, unsigned line) {
  std::vector<std::int64_t> result;
  for (const auto &node : graph.nodes)
    if (node.kind == kind && node.location.line == line)
      result.push_back(node.id);
  return result;
}

bool dataEdge(const facts::variableflow::Graph &graph, std::int64_t source,
              std::int64_t target) {
  return std::ranges::any_of(graph.edges, [&](const auto &edge) {
    return edge.kind == "data" && edge.source == source &&
           edge.target == target;
  });
}

void lastWriteWins() {
  TempSource source(R"cpp(
int root() {
  int x = 1;
  x = 2;
  return x;
}
)cpp");
  const auto graph = analyse(source);
  assert(graph.has_value());
  const auto first = nodesAt(*graph, "write", 3);
  const auto second = nodesAt(*graph, "write", 4);
  const auto returned = nodesAt(*graph, "read", 5);
  assert(first.size() == 1 && second.size() == 1 && returned.size() == 1);
  assert(dataEdge(*graph, second.front(), returned.front()));
  assert(!dataEdge(*graph, first.front(), returned.front()));
}

void branchJoin() {
  TempSource source(R"cpp(
int root(bool flag) {
  int x = 0;
  if (flag) {
    x = 1;
  } else {
    x = 2;
  }
  return x;
}
)cpp");
  const auto graph = analyse(source);
  assert(graph.has_value());
  const auto left = nodesAt(*graph, "write", 5);
  const auto right = nodesAt(*graph, "write", 7);
  const auto returned = nodesAt(*graph, "read", 9);
  assert(left.size() == 1 && right.size() == 1 && returned.size() == 1);
  assert(dataEdge(*graph, left.front(), returned.front()));
  assert(dataEdge(*graph, right.front(), returned.front()));
}

void loopBackedge() {
  TempSource source(R"cpp(
int root() {
  int x = 0;
  while (x < 3) {
    x += 1;
  }
  return x;
}
)cpp");
  const auto graph = analyse(source);
  assert(graph.has_value());
  const auto update = nodesAt(*graph, "update", 5);
  assert(update.size() == 1);
  assert(dataEdge(*graph, update.front(), update.front()));
}

void rhsBeforeAssignmentWrite() {
  TempSource source(R"cpp(
int root() {
  int x = 1;
  x = x + 1;
  return x;
}
)cpp");
  const auto graph = analyse(source);
  assert(graph.has_value());
  const auto init = nodesAt(*graph, "write", 3);
  const auto rhs = nodesAt(*graph, "read", 4);
  const auto assignment = nodesAt(*graph, "write", 4);
  assert(init.size() == 1 && rhs.size() == 1 && assignment.size() == 1);
  assert(dataEdge(*graph, init.front(), rhs.front()));
  assert(!dataEdge(*graph, assignment.front(), rhs.front()));
}

} // namespace

int main() {
  lastWriteWins();
  branchJoin();
  loopBackedge();
  rhsBeforeAssignmentWrite();
}
