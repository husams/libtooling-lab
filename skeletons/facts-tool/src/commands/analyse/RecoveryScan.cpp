#include "commands/analyse/RecoveryScan.h"
#include "commands/analyse/CallGraphRecoveryInternal.h"
#include "commands/analyse/RecoveryCompilation.h"
#include "cli/Verbose.h"
#include <clang/Basic/DiagnosticOptions.h>
#include <clang/Frontend/TextDiagnosticPrinter.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/Support/raw_ostream.h>

namespace facts::commands {
// Compiler diagnostics are collected instead of printed so a quiet run keeps
// its one-line stderr contract; verbose runs echo them after the compile.
struct CapturedDiagnostics {
  std::string text;
  llvm::raw_string_ostream stream{text};
  clang::DiagnosticOptions options;
  clang::TextDiagnosticPrinter printer{stream, options};
};

std::string recoveryScanDiagnostics(const RecoveryScan &scan) {
  if (!scan.diagnostics)
    return {};
  scan.diagnostics->stream.flush();
  return scan.diagnostics->text;
}

std::string recoveryScanDiagnostics(const RecoveryContext &context,
                                    FileId id) {
  const auto found = context.scans.find(id);
  return found == context.scans.end() ? std::string{}
                                      : recoveryScanDiagnostics(*found->second);
}

bool recoveryScanCurrent(RecoveryContext &context, const RecoveryScan &scan) {
  if (scan.registry != recoveryRegistryFingerprint(context) ||
      scan.availability != recoveryInputAvailability(context))
    return false;
  const auto digest = context.digests.digest(scan.inputs);
  return digest && *digest == scan.digest;
}

std::shared_ptr<RecoveryScan>
prepareRecoveryScan(RecoveryContext &context,
                    const RecoveryCandidate &candidate) {
  const auto id = candidate.entry.tuFileId;
  if (const auto found = context.scans.find(id);
      found != context.scans.end() &&
      recoveryScanCurrent(context, *found->second)) {
    return found->second;
  }
  auto scan = std::make_shared<RecoveryScan>();
  scan->registry = recoveryRegistryFingerprint(context);
  scan->availability = recoveryInputAvailability(context);
  if (candidate.entry.arguments.empty()) {
    scan->error = candidate.entry.reason;
  } else {
    RecoveryCompilation database(candidate);
    const std::vector<std::string> sources{candidate.source.string()};
    clang::tooling::ClangTool tool(database, sources);
    tool.clearArgumentsAdjusters();
    scan->diagnostics = std::make_shared<CapturedDiagnostics>();
    tool.setDiagnosticConsumer(&scan->diagnostics->printer);
    scan->status = tool.buildASTs(scan->units);
    if (scan->units.empty())
      scan->status = 1;
    for (const auto &unit : scan->units)
      if (unit->getDiagnostics().hasErrorOccurred())
        scan->status = 1;
    if (context.verbosity >= 1)
      llvm::errs() << recoveryScanDiagnostics(*scan);
  }
  collectRecoveryScanInputs(context, candidate, *scan);
  if (const auto digest = context.digests.digest(scan->inputs))
    scan->digest = *digest;
  collectRecoveryScanBodies(context, *scan);
  context.scans[id] = scan;
  return scan;
}
} // namespace facts::commands
