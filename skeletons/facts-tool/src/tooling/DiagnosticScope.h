#pragma once
#include "model/AnalysisDiagnostic.h"
#include "tooling/ExecutionTrace.h"
#include <clang/Basic/Diagnostic.h>

namespace clang::tooling { class ClangTool; }
namespace facts {
class DiagnosticScope final : public clang::DiagnosticConsumer {
public:
  DiagnosticScope();
  ~DiagnosticScope() override;
  DiagnosticScope(const DiagnosticScope &) = delete;
  DiagnosticScope &operator=(const DiagnosticScope &) = delete;
  void HandleDiagnostic(clang::DiagnosticsEngine::Level level,
                         const clang::Diagnostic &diagnostic) override;
  const std::vector<AnalysisDiagnostic> &messages() const { return messages_; }
  const ExecutionTrace &trace() const { return trace_; }
private:
  ExecutionTrace trace_;
  ExecutionTrace *previousTrace_;
  DiagnosticScope *previous_;
  std::vector<AnalysisDiagnostic> *previousMessages_;
  std::vector<AnalysisDiagnostic> messages_;
};
void configureDiagnostics(clang::tooling::ClangTool &tool);
}
