#pragma once

#include "model/SymbolId.h"

#include <string>
#include <vector>

namespace facts::callgraph {

struct RecoveryEntry {
  FileId tuFileId = 0;
  std::string component;
  std::string driver;
  std::string workingDirectory;
  std::vector<std::string> arguments;
  std::vector<std::string> relatedUsrs;
  std::string reason;
};

struct RecoveryReport {
  bool requested = false;
  std::vector<RecoveryEntry> attempted;
  std::vector<RecoveryEntry> reused;
  std::vector<RecoveryEntry> failed;
  std::vector<RecoveryEntry> suppressed;
};

} // namespace facts::callgraph
