#pragma once
#include "apis/operations/Operations.h"
#include "tooling/DiagnosticScope.h"
#include "storage/SqliteQuery.h"

namespace facts::apis::operations {
inline nlohmann::json diagnosticResults(const DiagnosticScope &scope) {
  auto result = nlohmann::json::array();
  for (const auto &item : scope.messages())
    result.push_back({{"severity", item.severity}, {"message", item.message},
                      {"file", item.file}, {"line", item.line},
                      {"column", item.column}});
  return result;
}
template <typename Work>
Result withDiagnostics(Work work) {
  DiagnosticScope scope;
  auto result = [&]() -> Result {
    try { return work(); }
    catch (const storage::QueryError &error) {
      return std::unexpected(domain::Error{500, "storage_error", error.what()});
    } catch (const std::exception &error) {
      return std::unexpected(domain::Error{500, "operation_failed", error.what()});
    }
  }();
  auto diagnostics = diagnosticResults(scope);
  if (result) (*result)["diagnostics"] = std::move(diagnostics);
  else result.error().details = {{"diagnostics", std::move(diagnostics)}};
  return result;
}
}
