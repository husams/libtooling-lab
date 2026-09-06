#pragma once

#include "analysis/callgraph/CallGraphQuery.h"
#include "model/CallGraphEntry.h"

#include <optional>
#include <string>
#include <vector>

namespace facts::callgraph {

struct ExternalTarget {
  SymbolId id;
  std::string name;
  std::string usr;
  RelationKind kind = RelationKind::Calls;
  unsigned position = 0;
  FileId file = builtinFileId;
  unsigned offset = 0;
  unsigned line = 0;
  unsigned column = 0;
};

struct EntryRecord {
  std::optional<CallGraphEntry> entry;
  std::vector<ExternalTarget> externalTargets;
  bool leaf = false;
};

std::expected<EntryRecord, std::string>
loadCallGraphEntry(const std::string &path, SymbolId symbol);

std::string renderCallGraphEntryText(const QueryNode &node,
                                     const EntryRecord &record);
std::string renderCallGraphEntryJson(const QueryNode &node,
                                     const EntryRecord &record);

} // namespace facts::callgraph
