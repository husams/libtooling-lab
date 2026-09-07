#include "analysis/callgraph/CallGraphJson.h"
#include "analysis/callgraph/CallGraphMermaid.h"
#include "commands/analyse/CallGraphOutput.h"
#include "commands/analyse/CallGraphResult.h"
#include <llvm/Support/JSON.h>
#include <llvm/Support/raw_ostream.h>

namespace facts::commands {
std::expected<int, std::string> publishCallGraph(
    const cli::CallGraphOptions &options, const CallGraphRequest &request,
    const callgraph::QueryGraph &graph,
    const callgraph::CoverageReport *coverage, const CallGraphResult &result,
    const callgraph::RecoveryReport *recovery, bool initial) {
  const auto view = options.edges == "calls" ? callgraph::EdgeView::Calls
                                             : callgraph::EdgeView::Semantic;
  auto text = result.traversal.text;
  if (options.format != "text") {
    text = callgraph::renderCallGraphJson(
        graph, result.roots, result.traversal, coverage, view, request.mode,
        request.mode == callgraph::QueryMode::Path
            ? std::optional{request.pathMode}
            : std::nullopt,
        result.target, result.paths, result.pathResult, recovery);
    auto metadata = llvm::json::parse(text);
    if (!metadata)
      return std::unexpected("cannot serialize graph metadata");
    auto &object = *metadata->getAsObject();
    object["provenance"] = llvm::json::Object{
        {"facts", options.facts}, {"project", options.configuration}};
    llvm::json::Array selectedRoots;
    for (const auto *root : result.roots)
      selectedRoots.push_back(
          llvm::json::Object{{"usr", root->usr}, {"name", root->name}});
    object["selected_roots"] = std::move(selectedRoots);
    object["artifact_stage"] = initial ? "initial" : "final";
    if (initial)
      object["recovery_pending"] = true;
    text.clear();
    llvm::raw_string_ostream stream(text);
    stream << *metadata << '\n';
    if (options.format == "mermaid")
      text = callgraph::renderCallGraphMermaid(
          graph, result.traversal, text, options.facts, coverage, view, initial,
          recovery && !recovery->failed.empty());
  }
  if (options.format == "text" && recovery)
    text += "recovery-requested=true recovery-failed=" +
            std::to_string(recovery->failed.size()) + "\n";
  return writeGraphOutput(options.output, text).transform([&] {
    return result.traversal.reason == "cancelled" ? 130 : 0;
  });
}
} // namespace facts::commands
