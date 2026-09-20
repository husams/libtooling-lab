#include "analysis/variableflow/Engine.h"

#include "analysis/variableflow/Internal.h"

namespace facts::variableflow {

std::expected<Graph, std::string>
analyse(clang::tooling::CompilationDatabase &database,
        const std::vector<std::string> &sources, const Request &request,
        const astcache::Options &astCache) {
  if (request.function.empty())
    return std::unexpected("function selector is required");
  if (request.variable.empty())
    return std::unexpected("variable selector is required");
  if (request.cancelled && request.cancelled())
    return std::unexpected("variable-flow cancelled");
  return detail::parse(database, sources, astCache, request.cancelled)
      .and_then([&](const detail::Parsed &parsed) -> std::expected<Graph, std::string> {
        if (request.cancelled && request.cancelled())
          return std::unexpected("variable-flow cancelled");
        return detail::selectFunction(parsed, request)
            .and_then([&](const detail::Function *function) {
              return detail::selectVariable(*function, request)
                  .and_then([&](const clang::VarDecl *variable) -> std::expected<Graph, std::string> {
                    auto result = detail::runFlow(parsed, *function, variable, request);
                    if (result.status == "cancelled")
                      return std::unexpected("variable-flow cancelled");
                    return result;
                  });
            });
      });
}

} // namespace facts::variableflow
