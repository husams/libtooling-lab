#pragma once

#include "commands/analyse/CallGraphRecovery.h"
#include "commands/analyse/RecoveryAttempts.h"
#include "commands/analyse/RecoveryEvidence.h"
#include "storage/catalog/Records.h"
#include "tooling/StoredCompilationReader.h"

#include <filesystem>
#include <map>
#include <set>
#include <span>
#include <vector>

namespace facts::commands {

struct RecoveryCandidate {
  RecoveryEntry entry;
  std::filesystem::path source;
};

struct RecoveryContext {
  std::string project;
  std::map<FileId, catalog::File> files;
  std::map<FileId, StoredCompileFile> commands;
  StoredCommandAliases aliases;
  std::map<std::string, std::vector<FileId>> index;
  std::map<FileId, std::vector<FileId>> includes;
  std::set<std::string> preservedUsrs;
  std::set<std::string> reusedUsrs;
  std::map<std::string, FileId> reusedOwners;
  std::map<FileId, std::vector<recovery::RegisteredInput>> inputClosures;
  std::map<FileId, std::string> closureDigests;
  recovery::InputDigestCache digests;
};

std::expected<RecoveryContext, std::string>
loadRecoveryContext(const cli::CallGraphOptions &options);

std::expected<std::vector<RecoveryCandidate>, std::string>
selectRecoveryCandidates(const RecoveryContext &context,
                         const callgraph::QueryGraph &graph,
                         const callgraph::CoverageReport *coverage,
                         std::span<const SymbolId> reachable);

struct RecoveryAttemptResult {
  bool succeeded = false;
  std::string reason;
};

struct RecoveryProbeResult {
  int status = 0;
  std::set<std::string> matched;
};

std::vector<RecoveryEntry>
collectRecoveryReuseReport(RecoveryContext &context,
                           const callgraph::QueryGraph &graph);

std::string recoveryRegistryFingerprint(const RecoveryContext &context);
std::vector<recovery::RegisteredInput>
recoveryRegisteredInputs(RecoveryContext &context,
                         const RecoveryCandidate &candidate);

std::expected<RecoveryProbeResult, std::string>
probeRecoveryCandidate(const RecoveryContext &context,
                       const RecoveryCandidate &candidate);

std::expected<RecoveryAttemptResult, std::string>
extractRecoveryCandidate(const RecoveryContext &context,
                         const cli::CallGraphOptions &options,
                         const RecoveryCandidate &candidate,
                         const RecoveryEvidence *retained = nullptr);

std::expected<bool, std::string> processRecoveryCandidates(
    RecoveryContext &context, const cli::CallGraphOptions &options,
    std::vector<RecoveryCandidate> candidates, RecoveryReport &report,
    recovery::AttemptCache &cache, callgraph::QueryGraph &graph);

} // namespace facts::commands
