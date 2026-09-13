#include "analysis/variableflow/Engine.h"

#include <clang/Tooling/CompilationDatabase.h>

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

class TempSource {
public:
  explicit TempSource(std::string source) {
    const auto stamp =
        std::chrono::steady_clock::now().time_since_epoch().count();
    directory_ = std::filesystem::temp_directory_path() /
                 ("facts-variable-flow-" + std::to_string(stamp));
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

void writesAndNestedReturn() {
  TempSource source(R"cpp(
int leaf(int value) { return value + 1; }
int make(int value) { return leaf(value); }
void sink(int &value) { value += 2; }
int root() {
  int value = make(1);
  if (value > 0) value = leaf(value);
  sink(value);
  return value;
}
)cpp");

  clang::tooling::FixedCompilationDatabase database(".", {"-std=c++17"});
  const auto graph = facts::variableflow::analyse(
      database, {source.path().string()},
      {"root", "value", std::nullopt, std::nullopt});
  assert(graph.has_value());
  assert(std::ranges::any_of(
      graph->nodes, [](const auto &node) { return node.kind == "read"; }));
  assert(std::ranges::any_of(
      graph->nodes, [](const auto &node) { return node.kind == "write"; }));
  assert(std::ranges::any_of(
      graph->nodes, [](const auto &node) { return node.kind == "return"; }));
  assert(std::ranges::any_of(graph->edges, [](const auto &edge) {
    return edge.kind == "call-result";
  }));
}

void depthBoundary() {
  TempSource source("int child(int x) { return x; }\n"
                    "int root() { int x = 1; return child(x); }\n");
  clang::tooling::FixedCompilationDatabase database(".", {"-std=c++17"});
  const auto graph = facts::variableflow::analyse(
      database, {source.path().string()}, {"root", "x", std::nullopt, 0});
  assert(graph.has_value());
  assert(std::ranges::any_of(graph->boundaries, [](const auto &boundary) {
    return boundary.reason == "depth-limit";
  }));
}

void preciseAcceptanceFixtures() {
  TempSource source(R"cpp(
namespace precise {
int leaf(int x) { return x + 1; }
int supplier() { int result = leaf(1); int noise = leaf(999); return result; }
int copies(int input) { int copy = input; copy = leaf(copy); return copy; }
int multiple() { int value = supplier(); value = supplier(); return value; }
int expression(int input) { return leaf(input + 1); }
void bypointer(int *p) { p = nullptr; }
void pointee(int *p) { *p = 42; }
int alias() { int value = 1; bypointer(&value); pointee(&value); return value; }
int reference() { int value = 1; int &ref = value; ref += 3; return value; }
}
)cpp");
  clang::tooling::FixedCompilationDatabase database(".", {"-std=c++17"});
  const auto run = [&](std::string function, std::string variable) {
    return facts::variableflow::analyse(
        database, {source.path().string()},
        {std::move(function), std::move(variable), std::nullopt, std::nullopt});
  };
  const auto copies = run("precise::copies", "copy");
  assert(copies.has_value());
  assert(std::ranges::any_of(copies->nodes, [](const auto &node) {
    return node.kind == "call" && node.name == "precise::leaf";
  }));
  const auto input = run("precise::copies", "input");
  assert(input.has_value());
  assert(std::ranges::any_of(input->nodes, [](const auto &node) {
    return node.kind == "write" && node.name == "copy";
  }));
  const auto expression = run("precise::expression", "input");
  assert(expression.has_value());
  assert(std::ranges::any_of(expression->nodes, [](const auto &node) {
    return node.kind == "call" && node.name == "precise::leaf";
  }));
  const auto multiple = run("precise::multiple", "value");
  assert(multiple.has_value());
  assert(std::ranges::count_if(multiple->nodes, [](const auto &node) {
           return node.kind == "call" && node.name == "precise::supplier";
         }) == 2);
  assert(std::ranges::count_if(multiple->edges, [](const auto &edge) {
           return edge.kind == "return";
         }) >= 2);
  const auto alias = run("precise::alias", "value");
  assert(alias.has_value());
  assert(std::ranges::any_of(alias->edges, [](const auto &edge) {
    return edge.kind == "reference-effect";
  }));
  const auto reference = run("precise::reference", "value");
  assert(reference.has_value());
  assert(std::ranges::any_of(reference->nodes, [](const auto &node) {
    return node.kind == "update";
  }));
}

} // namespace

int main() {
  writesAndNestedReturn();
  depthBoundary();
  preciseAcceptanceFixtures();
}
