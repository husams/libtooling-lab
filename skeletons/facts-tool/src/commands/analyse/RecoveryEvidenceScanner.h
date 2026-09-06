#pragma once

#include <expected>
#include <map>
#include <string>
#include <vector>

namespace facts::commands {
struct RecoveryCandidate;

struct RecoveryDefinitionProof {
  std::string path;
  unsigned offset = 0;
  unsigned size = 0;
};

struct RecoveryCallSite {
  std::string destination;
  std::string path;
  unsigned offset = 0;
  unsigned line = 0;
  unsigned column = 0;
};

struct RecoveryBodyFacts {
  std::map<std::string, std::vector<RecoveryCallSite>> calls;
  std::map<std::string, RecoveryDefinitionProof> definitions;
  bool unsupported = false;
};

std::expected<RecoveryBodyFacts, std::string>
scanRecoveryBody(const RecoveryCandidate &candidate);
} // namespace facts::commands
