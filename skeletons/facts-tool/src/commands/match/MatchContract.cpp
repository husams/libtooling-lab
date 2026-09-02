#include "commands/match/MatchContract.h"

#include "commands/match/RelationKinds.h"

#include <clang/AST/Expr.h>

#include <set>

namespace facts::commands::match {
namespace {
using Map = clang::ast_matchers::BoundNodes::IDToNodeMap;

bool exactKeys(const Map &nodes, std::initializer_list<std::string_view> keys) {
  const std::set<std::string, std::less<>> expected(keys.begin(), keys.end());
  if (nodes.size() != expected.size())
    return false;
  for (const auto &[key, value] : nodes)
    if (!expected.contains(key))
      return false;
  return true;
}

std::expected<Contract, std::string>
relationContract(const Map &nodes, RelationKind kind, bool hasSite) {
  auto *source = nodes.at("source").get<clang::NamedDecl>();
  auto *target = nodes.at("target").get<clang::NamedDecl>();
  if (!source || !target)
    return std::unexpected("source and target bindings must be declarations");
  const auto *stmt = hasSite ? nodes.at("site").get<clang::Stmt>() : nullptr;
  const auto *decl =
      hasSite ? nodes.at("site").get<clang::NamedDecl>() : nullptr;
  if (hasSite && !stmt && !decl)
    return std::unexpected("site binding must be a statement or declaration");
  if (siteBacked(kind) && !hasSite)
    return std::unexpected(std::string{relationName(kind)} +
                           " requires site binding");
  if (kind == RelationKind::Overrides && !decl)
    return std::unexpected("Overrides site must bind its source declaration");
  if (!siteBacked(kind) && hasSite)
    return std::unexpected(std::string{relationName(kind)} +
                           " forbids site binding");
  if (kind == RelationKind::Calls)
    return std::unexpected("Calls requires call and callee bindings");
  return Contract{RelationMatch{*source, *target, stmt, decl, kind}};
}
} // namespace

std::expected<Contract, std::string>
classify(const clang::ast_matchers::BoundNodes &bound,
         const std::optional<std::string> &relationKind) {
  const auto &nodes = bound.getMap();
  if (exactKeys(nodes, {"symbol"})) {
    if (relationKind)
      return std::unexpected("symbol binding forbids --relation-kind");
    auto *symbol = nodes.at("symbol").get<clang::NamedDecl>();
    return symbol ? Contract{SymbolMatch{*symbol}}
                  : std::expected<Contract, std::string>{std::unexpected(
                        "symbol binding must be a supported declaration")};
  }
  if (exactKeys(nodes, {"call", "callee"})) {
    if (relationKind && *relationKind != "Calls")
      return std::unexpected("call and callee bindings only support Calls");
    auto *call = nodes.at("call").get<clang::CallExpr>();
    auto *callee = nodes.at("callee").get<clang::FunctionDecl>();
    return call && callee
               ? Contract{DirectCallMatch{*call, *callee}}
               : std::expected<Contract, std::string>{
                     std::unexpected("call must bind CallExpr and callee must "
                                     "bind FunctionDecl")};
  }
  const bool noSite = exactKeys(nodes, {"source", "target"});
  const bool withSite = exactKeys(nodes, {"source", "target", "site"});
  if (!noSite && !withSite)
    return std::unexpected("bindings must exactly match a supported contract");
  if (!relationKind)
    return std::unexpected(
        "source and target bindings require --relation-kind");
  return parseRelationKind(*relationKind).and_then([&](RelationKind kind) {
    return relationContract(nodes, kind, withSite);
  });
}

} // namespace facts::commands::match
