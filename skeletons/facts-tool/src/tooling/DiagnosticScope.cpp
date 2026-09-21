#include "tooling/DiagnosticScope.h"
#include <clang/Basic/SourceManager.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/ADT/SmallString.h>
#include <array>

namespace facts {
namespace {
thread_local DiagnosticScope *active = nullptr;
}
DiagnosticScope::DiagnosticScope()
    : previousTrace_(executionTrace), previous_(active), previousMessages_(detail::analysisDiagnostics) {
  executionTrace = &trace_;
  active = this;
  detail::analysisDiagnostics = &messages_;
}
DiagnosticScope::~DiagnosticScope() {
  executionTrace = previousTrace_;
  active = previous_;
  detail::analysisDiagnostics = previousMessages_;
}
void configureDiagnostics(clang::tooling::ClangTool &tool) {
  if (!active) return;
  tool.setDiagnosticConsumer(active);
  tool.setPrintErrorMessage(false);
  // CompilerInstance prints its own summary when ShowCarets is enabled,
  // independently of the diagnostic consumer. Disable only presentation;
  // keep diagnostic counts intact so compiler errors still fail the job.
  tool.appendArgumentsAdjuster(clang::tooling::getInsertArgumentAdjuster(
      "-fno-caret-diagnostics", clang::tooling::ArgumentInsertPosition::END));
}
void DiagnosticScope::HandleDiagnostic(clang::DiagnosticsEngine::Level level,
                                       const clang::Diagnostic &diagnostic) {
  clang::DiagnosticConsumer::HandleDiagnostic(level, diagnostic);
  constexpr std::array levels{"ignored", "note", "remark", "warning", "error", "fatal"};
  llvm::SmallString<256> message;
  diagnostic.FormatDiagnostic(message);
  AnalysisDiagnostic record{levels.at(static_cast<unsigned>(level)), message.str().str(), ""};
  if (diagnostic.hasSourceManager() && diagnostic.getLocation().isValid()) {
    const auto &source = diagnostic.getSourceManager();
    const auto location = source.getPresumedLoc(diagnostic.getLocation());
    if (location.isValid()) {
      record.file = location.getFilename();
      record.line = location.getLine();
      record.column = location.getColumn();
    }
  }
  collectDiagnostic(std::move(record));
}
}
