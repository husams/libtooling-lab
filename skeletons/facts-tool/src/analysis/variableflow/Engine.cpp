#include "analysis/variableflow/Engine.h"

#include "analysis/variableflow/Internal.h"

namespace facts::variableflow {

std::expected<Graph, std::string>
analyse(clang::tooling::CompilationDatabase &database,
        const std::vector<std::string> &sources, const Request &request) {
  if (request.function.empty())
    return std::unexpected("function selector is required");
  if (request.variable.empty())
    return std::unexpected("variable selector is required");
  return detail::parse(database, sources)
      .and_then([&](const detail::Parsed &parsed) {
        return detail::selectFunction(parsed, request)
            .and_then([&](const detail::Function *function) {
              return detail::selectVariable(*function, request)
                  .transform([&](const clang::VarDecl *variable) {
                    return detail::runFlow(parsed, *function, variable,
                                           request);
                  });
            });
      });
}

} // namespace facts::variableflow
