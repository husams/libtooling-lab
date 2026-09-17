#pragma once

#include <expected>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <tuple>
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

// Target USR (when the operand has a declaration), path, source position,
// declared callable type and source expression must all survive reuse.
using RecoveryPointerCallKey =
    std::tuple<std::optional<std::string>, std::string, unsigned, unsigned,
               unsigned, std::string, std::string>;
using RecoveryPointerEvidence =
    std::map<std::string, std::set<RecoveryPointerCallKey>>;

struct RecoveryBodyFacts {
  std::map<std::string, std::vector<RecoveryCallSite>> calls;
  std::map<std::string, RecoveryDefinitionProof> definitions;
  std::map<std::string, unsigned> unresolved;
  bool unsupported = false;
  RecoveryPointerEvidence pointerCalls;
};

std::expected<RecoveryBodyFacts, std::string>
scanRecoveryBody(const RecoveryContext &context,
                 const RecoveryCandidate &candidate);
} // namespace facts::commands
