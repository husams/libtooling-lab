#pragma once
#include <string>
#include <vector>

namespace facts {
struct AnalysisDiagnostic {
  std::string severity;
  std::string message;
  std::string file;
  unsigned line = 0;
  unsigned column = 0;
};
namespace detail {
inline thread_local std::vector<AnalysisDiagnostic> *analysisDiagnostics = nullptr;
}
inline bool embeddedAnalysis() { return detail::analysisDiagnostics != nullptr; }
inline bool collectDiagnostic(AnalysisDiagnostic diagnostic) {
  auto *sink = detail::analysisDiagnostics;
  if (!sink) return false;
  if (sink->size() == 256)
    sink->push_back({"warning", "Additional diagnostics omitted", "", 0, 0});
  if (sink->size() >= 256) return true;
  if (diagnostic.message.size() > 2048)
    diagnostic.message = diagnostic.message.substr(0, 2048) + "...";
  sink->push_back(std::move(diagnostic));
  return true;
}
}
