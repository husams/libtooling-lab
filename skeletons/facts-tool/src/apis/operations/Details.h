#pragma once
#include "apis/operations/Operations.h"
#include "cli/Options.h"
#include "model/SymbolId.h"
#include <llvm/Support/JSON.h>

namespace facts::apis::operations {
inline domain::Error operationError(std::string message) {
  return {422, "operation_failed", std::move(message)};
}
inline domain::Error storageError(std::string message) {
  return {500, "storage_error", std::move(message)};
}
domain::Result<void> completed(int status);
domain::Result<void> recordFacts(const domain::Context &context,
                                 const domain::ResolvedFile &file,
                                 bool invalidateExtraction = false,
                                 std::span<const FileId> refreshed = {});
nlohmann::json resultFile(const domain::ResolvedFile &file);
nlohmann::json resultBase(std::string_view operation,
                          const domain::ResolvedFile &file);
nlohmann::json jsonValue(const llvm::json::Value &value);
cli::ExtractOptions extractOptions(const domain::Context &context,
                                  const domain::ResolvedFile &file,
                                  const ExtractRequest &request);
cli::DependencyOptions dependencyOptions(const domain::Context &context,
                                        const domain::ResolvedFile &file);
cli::MatchOptions matchOptions(const domain::Context &context,
                              const domain::ResolvedFile &file,
                              const MatchRequest &request);
domain::Result<bool> needsExtraction(const domain::ResolvedFile &file);
Result extractionResult(const domain::ResolvedFile &file);
Result dependencyResult(const domain::ResolvedFile &file);
}
