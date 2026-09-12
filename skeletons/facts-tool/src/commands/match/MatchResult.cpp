#include "commands/match/MatchResult.h"

#include "cli/Options.h"
#include "commands/match/MatchResultLocation.h"

#include <clang/AST/ASTContext.h>
#include <clang/Basic/SourceManager.h>
#include <llvm/Support/raw_ostream.h>

namespace facts::commands::match {

llvm::json::Object describeMatch(
    const clang::ast_matchers::MatchFinder::MatchResult &result,
    const std::optional<std::string> &relationKind) {
  llvm::json::Object bindings;
  for (const auto &[name, node] : result.Nodes.getMap())
    bindings[name] = describeBinding(node, *result.Context);
  const auto &source = result.Context->getSourceManager();
  llvm::json::Value kind = nullptr;
  if (relationKind)
    kind = *relationKind;
  else if (result.Nodes.getNodeAs<clang::CallExpr>("call"))
    kind = "Calls";
  return llvm::json::Object{{"translation_unit", sourcePath(
               source, source.getLocForStartOfFile(source.getMainFileID()))},
          {"relation_kind", std::move(kind)},
          {"bindings", std::move(bindings)}};
}

namespace {
std::string nodeLocation(const clang::DynTypedNode &node,
                         const clang::ASTContext &context) {
  const auto binding = describeBinding(node, context);
  const auto *place = binding.getObject("location");
  if (!place)
    return " source=<unavailable>";
  return " source=" + place->getString("path")->str() + ":" +
         std::to_string(*place->getInteger("line")) + ":" +
         std::to_string(*place->getInteger("column"));
}
} // namespace

std::string describeLocation(const clang::NamedDecl &node,
                             const clang::ASTContext &context) {
  return nodeLocation(clang::DynTypedNode::create(node), context);
}

std::string describeLocation(const clang::Stmt &node,
                             const clang::ASTContext &context) {
  return nodeLocation(clang::DynTypedNode::create(node), context);
}

void writeResults(const cli::MatchOptions &options,
                  const std::vector<std::string> &sources,
                  llvm::json::Array matches, const std::string &text,
                  std::ostream &output) {
  if (options.format == "text") {
    output << text;
    return;
  }
  llvm::json::Array selected;
  for (const auto &source : sources)
    selected.push_back(source);
  llvm::json::Object document{
      {"schema_version", 1}, {"matcher", options.matcher},
      {"sources", std::move(selected)}, {"complete", true},
      {"facts_committed", true}, {"index_committed", true},
      {"matches", std::move(matches)}};
  std::string json;
  llvm::raw_string_ostream stream(json);
  stream << llvm::json::Value(std::move(document));
  output << json << '\n';
}

} // namespace facts::commands::match
