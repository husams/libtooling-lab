#pragma once

#include "analysis/callgraph/CallGraphSearch.h"
#include "model/SymbolId.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace facts::commands {

enum class RunStatus { Complete, Truncated, Cancelled, RecoveryFailed, Failed };

std::string_view runStatusName(RunStatus status);

struct RunRecoveryRow {
  FileId translationUnit = 0;
  std::string outcome;
  std::string diagnostic;
};

// Everything one `callgraph_run` row and its child rows need; ids and USRs
// only, so the record outlives the graph generation it was built from.
struct CallGraphRunRecord {
  std::string projectPath;
  std::string factsPath;
  callgraph::QueryMode mode = callgraph::QueryMode::Callees;
  std::optional<callgraph::PathMode> pathMode;
  callgraph::CallsScope callsScope = callgraph::CallsScope::all;
  std::vector<std::string> components;
  callgraph::TraversalLimits limits;
  bool recoverMissing = false;
  RunStatus status = RunStatus::Complete;
  std::string truncationReason;
  std::string error;
  std::vector<std::pair<SymbolId, std::string>> roots;
  std::optional<std::pair<SymbolId, std::string>> target;
  std::vector<callgraph::TraversedEdge> edges;
  std::vector<callgraph::FrontierNode> frontier;
  std::vector<RunRecoveryRow> recovery;
};

} // namespace facts::commands
