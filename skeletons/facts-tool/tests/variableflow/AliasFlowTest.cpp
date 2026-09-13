#include "analysis/variableflow/Engine.h"

#include <clang/Tooling/CompilationDatabase.h>

#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <optional>
#include <ranges>
#include <string>
#include <utility>

namespace {

class TempSource {
public:
  explicit TempSource(std::string text) {
    const auto stamp =
        std::chrono::steady_clock::now().time_since_epoch().count();
    directory_ = std::filesystem::temp_directory_path() /
                 ("facts-variable-flow-alias-" + std::to_string(stamp));
    std::filesystem::create_directories(directory_);
    path_ = directory_ / "source.cpp";
    std::ofstream(path_) << text;
  }

  ~TempSource() { std::filesystem::remove_all(directory_); }

  std::string path() const { return path_.string(); }

private:
  std::filesystem::path directory_;
  std::filesystem::path path_;
};

using Graph = facts::variableflow::Graph;

Graph analyse(clang::tooling::FixedCompilationDatabase &database,
              const TempSource &source, std::string function,
              std::string variable) {
  auto result = facts::variableflow::analyse(
      database, {source.path()},
      {std::move(function), std::move(variable), std::nullopt, std::nullopt});
  assert(result.has_value());
  return std::move(*result);
}

void referencesAndCompoundUpdates() {
  TempSource source(R"cpp(
int references() { int x = 1; int &alias = x; alias += 1; return x; }
)cpp");
  clang::tooling::FixedCompilationDatabase database(".", {"-std=c++17"});
  const auto original = analyse(database, source, "references", "x");
  const auto alias = analyse(database, source, "references", "alias");
  assert(std::ranges::any_of(
      original.nodes, [](const auto &node) { return node.kind == "update"; }));
  assert(std::ranges::any_of(
      alias.nodes, [](const auto &node) { return node.kind == "update"; }));
}

void pointerRebindAndPointee() {
  TempSource source(R"cpp(
int pointers() { int x = 1; int *p = &x; *p = 2; p = nullptr; return x; }
)cpp");
  clang::tooling::FixedCompilationDatabase database(".", {"-std=c++17"});
  const auto graph = analyse(database, source, "pointers", "x");
  const auto writes = std::ranges::count_if(graph.nodes, [](const auto &node) {
    return node.kind == "write" && node.name == "x";
  });
  assert(writes == 2);
}

void unknownAliasAndLambdaBoundaries() {
  TempSource source(R"cpp(
int unknown(bool choose) { int x = 1; int &r = choose ? x : x; r = 2; return x; }
int lambda() { int x = 1; auto f = [&] { x = 2; }; return x; }
)cpp");
  clang::tooling::FixedCompilationDatabase database(".", {"-std=c++17"});
  const auto unknown = analyse(database, source, "unknown", "x");
  const auto lambda = analyse(database, source, "lambda", "x");
  assert(std::ranges::any_of(unknown.boundaries, [](const auto &boundary) {
    return boundary.reason.find("alias") != std::string::npos;
  }));
  assert(std::ranges::any_of(lambda.boundaries,
                             [](const auto &) { return true; }));
  assert(!std::ranges::any_of(lambda.nodes, [](const auto &node) {
    return node.name == "operator()";
  }));
}

void uninitializedHasNoInventedDefinition() {
  TempSource source(R"cpp(
int uninitialized() { int x; return x; }
)cpp");
  clang::tooling::FixedCompilationDatabase database(".", {"-std=c++17"});
  const auto graph = analyse(database, source, "uninitialized", "x");
  const auto read = std::ranges::find_if(
      graph.nodes, [](const auto &node) { return node.kind == "read"; });
  assert(read != graph.nodes.end());
  assert(!std::ranges::any_of(graph.edges, [&](const auto &edge) {
    return edge.kind == "data" && edge.target == read->id;
  }));
}

} // namespace

int main() {
  referencesAndCompoundUpdates();
  pointerRebindAndPointee();
  unknownAliasAndLambdaBoundaries();
  uninitializedHasNoInventedDefinition();
}
