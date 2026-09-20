#include "apis/v2/JobServices.h"
#include "apis/operations/Operations.h"
#include "commands/Extract.h"

namespace facts::apis::v2::jobs {
Result<Json> fileAnalysis(const domain::Context &context, const runtime::Request &request) {
  return selectFiles(context, request.options.at("selection"))
      .and_then([&](const auto &files) -> Result<Json> {
        Json result{{"files_selected", files.size()}, {"files_processed", 0},
                    {"files_skipped", 0}, {"files_failed", 0},
                    {"coverage", "complete"}, {"diagnostics", Json::array()}};
        Json items = Json::array();
        std::size_t symbols = 0;
        for (const auto &file : files) {
          auto allowed = checkpoint(request);
          if (!allowed) return std::unexpected(allowed.error());
          auto prepared = prepareFile(context, file);
          if (!prepared) return std::unexpected(prepared.error());
          commands::ExtractionStatistics statistics;
          auto operation = [&]() -> Result<Json> {
            if (request.operation == "v2.extract")
              return operations::extract(context, file, {request.options.value("force", false), &statistics});
            if (request.operation == "v2.dependencies") return operations::dependencies(context, file);
            operations::MatchRequest match{request.options.at("expression").get<std::string>()};
            if (request.options.contains("traversal")) match.traversal = request.options["traversal"].get<std::string>();
            if (request.options.contains("relation_kind")) match.relationKind = request.options["relation_kind"].get<std::string>();
            match.captureSource = request.options.value("capture_source", false);
            if (request.options.contains("bindings")) {
              const auto &bindings = request.options["bindings"];
              match.sourceBinding = bindings.value("source", "source");
              match.targetBinding = bindings.value("target", "target");
              match.siteBinding = bindings.value("site", "site");
              match.callBinding = bindings.value("call", "call");
              match.calleeBinding = bindings.value("callee", "callee");
            }
            return operations::match(context, file, match);
          }();
          if (!operation) return std::unexpected(operation.error());
          const auto processed = request.operation == "v2.extract" ? statistics.processed : 1;
          result["files_processed"] = result["files_processed"].get<std::size_t>() + processed;
          result["files_skipped"] = result["files_skipped"].get<std::size_t>() + statistics.skipped;
          for (auto &diagnostic : operation->at("diagnostics")) result["diagnostics"].push_back(std::move(diagnostic));
          if (request.operation == "v2.extract") {
            symbols += statistics.symbols;
            items.push_back({{"file_id", std::to_string(file.fileId)}, {"path", file.path.string()},
                             {"symbol_count", operation->value("symbol_count", std::size_t{})}});
          } else {
            auto &values = operation->at(request.operation == "v2.match" ? "matches" : "edges");
            for (auto &value : values) {
              if (request.operation == "v2.dependencies") {
                value["source_file_id"] = std::to_string(value["source_file_id"].template get<std::int64_t>());
                value["destination_file_id"] = std::to_string(value["destination_file_id"].template get<std::int64_t>());
              }
              items.push_back(std::move(value));
            }
          }
        }
        if (request.operation == "v2.extract") {
          result["symbols_written"] = symbols;
          result["files"] = std::move(items);
        } else if (request.operation == "v2.match") {
          result["match_count"] = items.size();
          result["matches"] = std::move(items);
        } else {
          result["edge_count"] = items.size();
          result["edges"] = std::move(items);
        }
        return result;
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
