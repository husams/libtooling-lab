#include "apis/v2/JobServices.h"
#include "apis/operations/Diagnostics.h"
#include "analysis/variableflow/Engine.h"
#include "commands/CompilationDatabase.h"
#include "tooling/StoredCompilationDatabase.h"

namespace facts::apis::v2::jobs {
namespace {
Json encode(const variableflow::Graph &graph) {
  Json result{{"root_function", graph.rootFunction}, {"root_variable", graph.rootVariable},
              {"status", graph.status}, {"coverage", graph.boundaries.empty() ? "complete" : "partial"},
              {"nodes", Json::array()}, {"edges", Json::array()}, {"boundaries", Json::array()}};
  for (const auto &node : graph.nodes)
    result["nodes"].push_back({{"id", node.id}, {"kind", node.kind},
        {"function_usr", node.functionUsr}, {"variable_usr", node.variableUsr},
        {"name", node.name}, {"type", node.type},
        {"location", {{"path", node.location.file}, {"line", node.location.line},
                      {"column", node.location.column}, {"offset", node.location.offset}}},
        {"block", node.block}, {"depth", node.depth}});
  for (const auto &edge : graph.edges)
    result["edges"].push_back({{"source", edge.source}, {"target", edge.target},
                               {"kind", edge.kind}, {"callsite", edge.callsite}});
  for (const auto &boundary : graph.boundaries)
    result["boundaries"].push_back({{"node", boundary.node}, {"reason", boundary.reason},
                                    {"detail", boundary.detail}, {"depth", boundary.depth}});
  result["node_count"] = graph.nodes.size();
  result["edge_count"] = graph.edges.size();
  return result;
}
Result<variableflow::Request> flowRequest(const domain::Context &context,
    const runtime::Request &request, const index::Symbol &function) {
  const auto &variable = request.options.at("variable");
  variableflow::Request result{function.usr, variable.at("name").get<std::string>(),
      {}, request.options.value("max_call_depth", 10U)};
  result.cancelled = request.cancelled;
  if (!request.options.value("interprocedural", true)) result.maxDepth = 0;
  if (variable.contains("declaration")) {
    const auto &decl = variable["declaration"];
    result.line = decl.at("line").get<unsigned>();
    if (decl.contains("column")) result.column = decl["column"].get<unsigned>();
    domain::FileSelector selector{decl.at("path").get<std::string>()};
    if (!function.repository.empty()) selector.repo = function.repository;
    auto file = domain::resolveFile(context, selector);
    if (!file) return std::unexpected(file.error());
    result.file = file->path.string();
  }
  return result;
}
}
Result<Json> variableFlow(const domain::Context &context, const runtime::Request &request) {
  return operations::withDiagnostics([&]() -> Result<Json> {
    return resolveSymbol(context, request.options.at("function"))
        .and_then([&](const index::Symbol &function) -> Result<Json> {
          const auto selection = request.options.value("selection", function.repository.empty()
              ? Json{{"type", "all"}}
              : Json{{"type", "repository"}, {"repository", function.repository}});
          return selectFiles(context, selection).and_then([&](const auto &files) -> Result<Json> {
            std::vector<std::string> sources;
            for (const auto &file : files) sources.push_back(file.path.string());
            auto controls = flowRequest(context, request, function);
            if (!controls) return std::unexpected(controls.error());
            return loadStoredCompilationDatabase(context.configuration.database.string(), sources)
                .transform_error(failed).and_then([&](auto database) -> Result<Json> {
                  auto adjusted = commands::appendExtraArguments(std::move(database),
                      context.configuration.extraArguments);
                  auto allowed = checkpoint(request);
                  if (!allowed) return std::unexpected(allowed.error());
                  return variableflow::analyse(*adjusted, sources, *controls, context.configuration.astCache)
                      .transform_error([&](std::string message) {
                        return message == "variable-flow cancelled"
                            ? domain::Error{409, "cancelled", std::move(message)} : failed(std::move(message));
                      }).transform(encode);
                });
          });
        });
  });
}
}
