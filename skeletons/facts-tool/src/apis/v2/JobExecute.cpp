#include "apis/v2/JobServices.h"
#include "apis/operations/Operations.h"
#include "commands/Extract.h"

namespace facts::apis::v2::jobs {
namespace {
operations::MatchRequest matchRequest(const Json &options) {
  operations::MatchRequest match{options.at("expression").get<std::string>()};
  if (options.contains("traversal")) match.traversal = options["traversal"].get<std::string>();
  if (options.contains("relation_kind")) match.relationKind = options["relation_kind"].get<std::string>();
  match.captureSource = options.value("capture_source", false);
  const auto bindings = options.value("bindings", Json::object());
  match.sourceBinding = bindings.value("source", "source");
  match.targetBinding = bindings.value("target", "target");
  match.siteBinding = bindings.value("site", "site");
  match.callBinding = bindings.value("call", "call");
  match.calleeBinding = bindings.value("callee", "callee");
  return match;
}
Result<Json> analyze(const domain::Context &context, const runtime::Request &request,
                     const domain::ResolvedFile &file, commands::ExtractionStatistics &statistics) {
  return prepareFile(context, file)
      .and_then([&]() -> Result<Json> {
        if (request.operation == "v2.extract")
          return operations::extract(context, file, {request.options.value("force", false), &statistics});
        if (request.operation == "v2.dependencies") return operations::dependencies(context, file);
        return operations::match(context, file, matchRequest(request.options));
      });
}
Json initialResult(const runtime::Request &request, std::size_t count) {
  Json result{{"files_selected", count}, {"files_processed", 0}, {"files_skipped", 0},
      {"files_failed", 0}, {"files_not_attempted", count}, {"coverage", "complete"},
      {"index_revision", nullptr}, {"diagnostics", Json::array()}, {"failed_files", Json::array()}};
  if (request.operation == "v2.extract") {
    result["symbols_written"] = 0;
    result["files"] = Json::array();
  } else if (request.operation == "v2.match") {
    result["match_count"] = 0;
    result["matches"] = Json::array();
  } else {
    result["edge_count"] = 0;
    result["edges"] = Json::array();
  }
  return result;
}
void appendDiagnostics(Json &result, const Json &document) {
  if (document.is_object() && document.contains("diagnostics"))
    for (const auto &diagnostic : document.at("diagnostics"))
      result["diagnostics"].push_back(diagnostic);
}
void collectSuccess(Json &result, const runtime::Request &request,
                    const domain::ResolvedFile &file, Json operation,
                    const commands::ExtractionStatistics &statistics) {
  const auto processed = request.operation == "v2.extract" ? statistics.processed : 1;
  result["files_processed"] = result["files_processed"].get<std::size_t>() + processed;
  result["files_skipped"] = result["files_skipped"].get<std::size_t>() + statistics.skipped;
  appendDiagnostics(result, operation);
  if (request.operation == "v2.extract") {
    result["symbols_written"] = result["symbols_written"].get<std::size_t>() + statistics.symbols;
    result["files"].push_back({{"file_id", std::to_string(file.fileId)}, {"path", file.path.string()},
        {"symbol_count", operation.value("symbol_count", std::size_t{})}});
    return;
  }
  const bool dependencies = request.operation == "v2.dependencies";
  const auto collection = dependencies ? "edges" : "matches";
  for (auto &value : operation.at(collection)) {
    if (dependencies) {
      value["source_file_id"] = std::to_string(value["source_file_id"].get<std::int64_t>());
      value["destination_file_id"] = std::to_string(value["destination_file_id"].get<std::int64_t>());
    }
    result[collection].push_back(std::move(value));
  }
  result[dependencies ? "edge_count" : "match_count"] = result[collection].size();
}
void collectFailure(Json &result, const runtime::Request &request,
                    const domain::ResolvedFile &file, domain::Error &error) {
  if (request.reportFileFailure) request.reportFileFailure(error, std::to_string(file.fileId));
  appendDiagnostics(result, error.details);
  Json encoded{{"code", error.code}, {"message", error.message}};
  if (!error.details.is_null()) encoded["details"] = error.details;
  result["failed_files"].push_back({{"file_id", std::to_string(file.fileId)},
      {"path", file.path.string()}, {"error", std::move(encoded)}});
  result["files_failed"] = result["failed_files"].size();
  result["coverage"] = "partial";
}
Result<void> processFile(const domain::Context &context, const runtime::Request &request,
                         const domain::ResolvedFile &file, Json &result) {
  commands::ExtractionStatistics statistics;
  auto operation = analyze(context, request, file, statistics);
  if (operation) {
    collectSuccess(result, request, file, std::move(*operation), statistics);
    return {};
  }
  if (operation.error().code == "cancelled") return std::unexpected(operation.error());
  collectFailure(result, request, file, operation.error());
  if (!request.options.value("continue_on_error", false)) return std::unexpected(operation.error());
  return {};
}
Result<void> requireCompletedFile(const Json &result) {
  if (result["files_failed"] == result["files_selected"])
    return std::unexpected(failed("Every selected file failed; inspect the failed_files result collection"));
  return {};
}
Result<Json> finish(Json result, const Result<void> &outcome) {
  result["files_not_attempted"] = result["files_selected"].get<std::size_t>() -
      result["files_processed"].get<std::size_t>() - result["files_skipped"].get<std::size_t>() -
      result["files_failed"].get<std::size_t>();
  const auto completed = outcome.and_then([&] { return requireCompletedFile(result); });
  if (completed) return result;
  result["coverage"] = "partial";
  auto error = completed.error();
  if (!error.details.is_object()) error.details = Json::object();
  error.details["partial_result"] = std::move(result);
  return std::unexpected(std::move(error));
}
}
Result<Json> fileAnalysis(const domain::Context &context, const runtime::Request &request) {
  return selectFiles(context, request.options.at("selection"))
      .and_then([&](const auto &files) -> Result<Json> {
        auto result = initialResult(request, files.size());
        for (const auto &file : files) {
          auto outcome = checkpoint(request)
              .and_then([&] { return processFile(context, request, file, result); });
          if (!outcome) return finish(std::move(result), outcome);
        }
        return finish(std::move(result), checkpoint(request));
      });
}
}
namespace facts::apis::v2 {
domain::Result<nlohmann::json> executeJob(const domain::Context &context,
                                         const runtime::Request &request) {
  return jobs::checkpoint(request).and_then([&]() -> jobs::Result<jobs::Json> {
    if (request.operation == "v2.index") return jobs::Json::object();
    if (request.operation == "v2.callgraphs") return jobs::callGraph(context, request);
    if (request.operation == "v2.variable-flow") return jobs::variableFlow(context, request);
    if (request.operation == "v2.import") return jobs::importCompilation(context, request);
    if (request.operation == "v2.scan") return jobs::scan(context, request);
    return jobs::fileAnalysis(context, request);
  });
}
}
