#pragma once

#include "model/SymbolId.h"

#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace facts::callgraph {

enum class CallsScope { all, project, library };

struct TraversalLimits {
  std::optional<int> depth;
  std::optional<std::uint64_t> nodes;
  std::optional<std::uint64_t> edges;
  std::optional<std::chrono::milliseconds> time;
};

struct ScopeSelection {
  std::vector<std::string> requestedComponents;
  CallsScope calls = CallsScope::all;
  std::set<std::int64_t> componentIds;

  bool active() const {
    return !requestedComponents.empty() || calls != CallsScope::all;
  }
};

struct TraversalRequest {
  ScopeSelection scope;
  TraversalLimits limits;
  std::function<bool()> cancelled;
};

struct FrontierNode {
  SymbolId id;
  std::string reason;
};

struct ExcludedBoundary {
  SymbolId source;
  SymbolId target;
  std::string reason;
};

std::string_view scopeName(CallsScope scope);

} // namespace facts::callgraph
