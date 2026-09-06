#pragma once

#include <expected>
#include <map>
#include <string>
#include <vector>

namespace facts::commands {
struct RecoveryCandidate;
struct RecoveryContext;

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
  bool implicit = false;
};

struct RecoveryBodyFacts {
  std::map<std::string, std::vector<RecoveryCallSite>> calls;
  std::map<std::string, RecoveryDefinitionProof> definitions;
  std::map<std::string, unsigned> unresolved;
  bool unsupported = false;
};

std::expected<RecoveryBodyFacts, std::string>
scanRecoveryBody(const RecoveryContext &context,
                 const RecoveryCandidate &candidate);
} // namespace facts::commands
