#pragma once
#include "apis/v2/Jobs.h"
#include "apis/watch/Scan.h"

namespace facts::apis::v2::jobs {
using Json = nlohmann::json;
template <class T> using Result = domain::Result<T>;
inline domain::Error invalid(std::string message) {
  return {422, "invalid_request", std::move(message)};
}
inline domain::Error failed(std::string message) {
  return {422, "analysis_failed", std::move(message)};
}
inline Result<void> checkpoint(const runtime::Request &request) {
  if (request.cancelled && request.cancelled())
    return std::unexpected(domain::Error{409, "cancelled", "Job was cancelled"});
  return {};
}
Result<void> validateSelection(const Json &selection);
Result<void> validateSymbol(const Json &selector);
Result<std::vector<domain::ResolvedFile>> selectFiles(const domain::Context &,
                                                     const Json &selection);
Result<watch::Catalog> selectClones(const domain::Context &,
                                   const runtime::Request &);
Result<index::Symbol> resolveSymbol(const domain::Context &, const Json &selector);
Result<void> prepareFile(const domain::Context &, const domain::ResolvedFile &);
Result<Json> fileAnalysis(const domain::Context &, const runtime::Request &);
Result<Json> callGraph(const domain::Context &, const runtime::Request &);
Result<Json> variableFlow(const domain::Context &, const runtime::Request &);
Result<Json> importCompilation(const domain::Context &, const runtime::Request &);
Result<Json> scan(const domain::Context &, const runtime::Request &);
}
