#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace facts::variableflow {

struct Request {
  std::string function;
  std::string variable;
  std::optional<unsigned> line;
  std::optional<unsigned> maxDepth;
  std::optional<std::string> file;
  std::optional<unsigned> column;
  std::function<bool()> cancelled;
};

struct Location {
  std::string file;
  unsigned line = 0;
  unsigned column = 0;
  std::uint64_t offset = 0;
};

struct Node {
  std::int64_t id = 0;
  std::string kind;
  std::string functionUsr;
  std::string variableUsr;
  std::string name;
  std::string type;
  Location location;
  int block = -1;
  unsigned depth = 0;
};

struct Edge {
  std::int64_t source = 0;
  std::int64_t target = 0;
  std::string kind;
  std::int64_t callsite = 0;
};

struct Boundary {
  std::int64_t node = 0;
  std::string reason;
  std::string detail;
  unsigned depth = 0;
};

struct Graph {
  std::string rootFunction;
  std::string rootVariable;
  std::string status;
  std::vector<Node> nodes;
  std::vector<Edge> edges;
  std::vector<Boundary> boundaries;
};

} // namespace facts::variableflow
