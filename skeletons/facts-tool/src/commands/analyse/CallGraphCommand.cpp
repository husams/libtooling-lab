#include "commands/analyse/CallGraphCommand.h"

#include "analysis/callgraph/CallGraphQuery.h"
#include "analysis/callgraph/CallGraphTraversal.h"

#include <iostream>

namespace facts::commands {

std::expected<int, std::string>
runCallGraph(const cli::CallGraphOptions &options) {
  return callgraph::loadCallGraph(options.facts)
      .and_then([&](const auto &graph) {
        return callgraph::selectRoots(graph, options.function, options.all)
            .transform([&](const auto &roots) {
              std::cout << callgraph::renderCallGraph(
                               graph, roots, options.maxDepth).text;
              return 0;
            });
      });
}

} // namespace facts::commands
