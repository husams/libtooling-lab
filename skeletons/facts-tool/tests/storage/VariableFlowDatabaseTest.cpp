#include "storage/variableflow/Database.h"

#include <cassert>
#include <filesystem>

using namespace facts::variableflow;

namespace {
RunMetadata metadata() {
  return {"project.db", "facts.db",   "main", "value",           {"main.cpp"},
          std::nullopt, std::nullopt, "test", "test assumptions"};
}

Graph valid() {
  Graph graph{"main", "value", "complete"};
  graph.nodes.push_back({1,
                         "definition",
                         "_Z4mainv",
                         "value",
                         "value",
                         "int",
                         {"main.cpp", 1, 1, 0},
                         0,
                         0});
  return graph;
}
} // namespace

int main(int argc, char **argv) {
  assert(argc == 2);
  const auto path = std::filesystem::path(argv[1]);
  std::error_code error;
  std::filesystem::remove(path, error);
  const auto first = persist(path, metadata(), valid());
  assert(first && *first == 1);

  auto invalid = valid();
  invalid.edges.push_back({1, 99, "assignment", 10});
  assert(!persist(path, metadata(), invalid));

  const auto second = persist(path, metadata(), valid());
  assert(second && *second == 2);
  return 0;
}
