#pragma once
#include "analysis/callgraph/CallGraphQuery.h"
#include "commands/analyse/RecoveryAttempts.h"
#include "commands/analyse/RecoveryEvidenceScanner.h"
#include <clang/Frontend/ASTUnit.h>
#include <memory>

namespace facts::commands {
struct RecoveryContext;
struct RecoveryCandidate;

struct CapturedDiagnostics;

struct RecoveryScan {
  // Owns the consumer the units still reference; declared first so it is
  // destroyed after them.
  std::shared_ptr<CapturedDiagnostics> diagnostics;
  std::vector<std::unique_ptr<clang::ASTUnit>> units;
  RecoveryBodyFacts facts;
  std::vector<recovery::RegisteredInput> inputs;
  std::string registry;
  std::string availability;
  std::string digest;
  std::string error;
  int status = 1;
  bool completeInputs = false;
};

std::string recoveryInputAvailability(const RecoveryContext &context);
// The front-end text captured while building the candidate's ASTs.
std::string recoveryScanDiagnostics(const RecoveryScan &scan);
std::string recoveryScanDiagnostics(const RecoveryContext &context, FileId id);
bool recoveryScanCurrent(RecoveryContext &context, const RecoveryScan &scan);
void collectRecoveryScanInputs(const RecoveryContext &,
                               const RecoveryCandidate &, RecoveryScan &);
void collectRecoveryScanBodies(const RecoveryContext &, RecoveryScan &);
callgraph::QueryResult collectRecoveryNativeFacts(const RecoveryContext &,
                                                  const RecoveryScan &);
std::shared_ptr<RecoveryScan> prepareRecoveryScan(RecoveryContext &,
                                                  const RecoveryCandidate &);
} // namespace facts::commands
