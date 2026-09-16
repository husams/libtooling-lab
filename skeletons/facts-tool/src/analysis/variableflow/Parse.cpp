#include "analysis/variableflow/Internal.h"

#include "platform/PlatformFlags.h"
#include "tooling/astcache/Cache.h"

#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Analysis/CallGraph.h>
#include <clang/Tooling/Tooling.h>

#include <algorithm>
#include <filesystem>
#include <memory>
#include <unordered_set>

namespace facts::variableflow::detail {
namespace {

class Definitions final : public clang::RecursiveASTVisitor<Definitions> {
public:
  Definitions(Parsed &parsed, clang::ASTContext &context, std::string tu)
      : parsed_(parsed), context_(context), tu_(std::move(tu)) {}

  bool VisitFunctionDecl(const clang::FunctionDecl *decl) {
    const auto *definition = decl->getDefinition();
    if (definition == nullptr || definition != decl)
      return true;
    if (context_.getSourceManager().isInSystemHeader(definition->getLocation()))
      return true;
    const auto usr = usrFor(*definition, tu_);
    if (!seen_.insert(usr).second)
      return true;
    auto function = std::make_unique<Function>(Function{
        .decl = definition, .context = &context_, .usr = usr, .tu = tu_});
    const auto *pointer = function.get();
    parsed_.functions.push_back(std::move(function));
    parsed_.byUsr[usr].push_back(pointer);
    parsed_.byName[functionName(*definition)].push_back(pointer);
    return true;
  }

private:
  Parsed &parsed_;
  clang::ASTContext &context_;
  std::string tu_;
  std::unordered_set<std::string> seen_;
};

std::string normalized(std::string source) {
  return std::filesystem::absolute(source).lexically_normal().string();
}

std::expected<void, std::string>
parseOne(clang::tooling::CompilationDatabase &database,
         const std::string &source, Parsed &parsed,
         const astcache::Options &astCache) {
  const std::vector<std::string> selected{source};
  const auto configured =
      facts::configurePlatformCompilationDatabase(database, selected);
  if (!configured)
    return std::unexpected(configured.error());
  std::vector<std::unique_ptr<clang::ASTUnit>> units;
  if (astcache::buildASTs(**configured, selected, units, astCache) != 0 ||
      units.empty())
    return std::unexpected("cannot build AST for source: " + source);
  for (auto &unit : units) {
    auto &context = unit->getASTContext();
    clang::CallGraph callGraph;
    callGraph.addToCallGraph(context.getTranslationUnitDecl());
    for (const auto &entry : callGraph) {
      for (const auto &call : *entry.second)
        if (call.CallExpr != nullptr)
          if (const auto *callee = llvm::dyn_cast_or_null<clang::FunctionDecl>(
                  call.Callee->getDecl()))
            parsed.callTargets.emplace(call.CallExpr, callee);
    }
    Definitions visitor(parsed, context, normalized(source));
    visitor.TraverseDecl(context.getTranslationUnitDecl());
    parsed.units.push_back(std::move(unit));
  }
  return {};
}

} // namespace

std::expected<Parsed, std::string>
parse(clang::tooling::CompilationDatabase &database,
      const std::vector<std::string> &requested,
      const astcache::Options &astCache) {
  Parsed parsed;
  std::vector<std::string> sources = requested;
  if (sources.empty())
    sources = database.getAllFiles();
  std::ranges::sort(sources);
  sources.erase(std::ranges::unique(sources).begin(), sources.end());
  for (const auto &source : sources) {
    if (auto result = parseOne(database, source, parsed, astCache); !result)
      return std::unexpected(result.error());
  }
  return parsed;
}

} // namespace facts::variableflow::detail
