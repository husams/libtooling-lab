#include "commands/match/MatchContract.h"

#include "commands/match/RelationKinds.h"
#include "commands/match/SymbolDispatch.h"

#include <clang/AST/Expr.h>

#include <set>

namespace facts::commands::match {
namespace {
using Map = clang::ast_matchers::BoundNodes::IDToNodeMap;

template <typename Node>
const Node *boundNode(const Map &nodes, const std::string &binding) {
  const auto found = nodes.find(binding);
  return found == nodes.end() ? nullptr : found->second.get<Node>();
}

std::expected<Contract, std::string>
relationContract(const Map &nodes, RelationKind kind,
                 const BindingNames &bindings) {
  const auto *source = boundNode<clang::NamedDecl>(nodes, bindings.source);
  const auto *target = boundNode<clang::NamedDecl>(nodes, bindings.target);
  if (!source || !target)
    return std::unexpected("source and target bindings must be declarations (" +
                           bindings.source + ", " + bindings.target + ")");
  const bool hasSite = nodes.contains(bindings.site);
  const auto *stmt = boundNode<clang::Stmt>(nodes, bindings.site);
  const auto *decl = boundNode<clang::NamedDecl>(nodes, bindings.site);
  if (hasSite && !stmt && !decl)
    return std::unexpected("site binding must be a statement or declaration");
  if (kind == RelationKind::Uses && !hasSite)
    return std::unexpected(std::string{relationName(kind)} +
                           " requires site binding");
  if (kind == RelationKind::Overrides && hasSite && !decl)
    return std::unexpected("Overrides site must bind a declaration");
  if (kind == RelationKind::Overrides && decl &&
      decl->getCanonicalDecl() != source->getCanonicalDecl())
    return std::unexpected("Overrides site must bind the source declaration");
  if (!siteBacked(kind) && hasSite)
    return std::unexpected(std::string{relationName(kind)} +
                           " forbids site binding");
  return Contract{RelationMatch{*source, *target, stmt, decl, kind}};
}

std::expected<Contract, std::string>
callContract(const Map &nodes, const BindingNames &bindings) {
  const auto *call = boundNode<clang::CallExpr>(nodes, bindings.call);
  const auto *callee = boundNode<clang::FunctionDecl>(nodes, bindings.callee);
  if (!call || !callee)
    return std::unexpected(
        "call must bind CallExpr and callee must bind FunctionDecl (" +
        bindings.call + ", " + bindings.callee + ")");
  return Contract{DirectCallMatch{*call, *callee}};
}

Contracts nodeContracts(const Map &nodes) {
  Contracts contracts;
  std::set<const void *> persisted;
  for (const auto &[name, node] : nodes) {
    if (const auto *decl = node.get<clang::NamedDecl>();
        decl && supportsSymbol(*decl)) {
      if (persisted.insert(decl).second)
        contracts.emplace_back(SymbolMatch{*decl});
    } else if (const auto *expression = node.get<clang::Expr>()) {
      if (persisted.insert(expression).second)
        contracts.emplace_back(ExpressionMatch{*expression});
    } else {
      contracts.emplace_back(NodeMatch{name, node});
    }
  }
  return contracts;
}
} // namespace

std::expected<Contracts, std::string>
classify(const Map &nodes,
         const std::optional<std::string> &relationKind,
         const BindingNames &bindings) {
  if (relationKind)
    return parseRelationKind(*relationKind)
        .and_then([&](RelationKind kind) {
          return kind == RelationKind::Calls
                     ? callContract(nodes, bindings)
                     : relationContract(nodes, kind, bindings);
        })
        .transform([](Contract contract) {
          return Contracts{std::move(contract)};
        });
  // Keep the existing direct-call shorthand, without reserving these names
  // for other node types or constraining additional helper bindings.
  if (boundNode<clang::CallExpr>(nodes, bindings.call) &&
      boundNode<clang::FunctionDecl>(nodes, bindings.callee))
    return callContract(nodes, bindings).transform([](Contract contract) {
      return Contracts{std::move(contract)};
    });
  return nodeContracts(nodes);
}

std::expected<Contracts, std::string>
classify(const clang::ast_matchers::BoundNodes &bound,
         const std::optional<std::string> &relationKind,
         const BindingNames &bindings) {
  return classify(bound.getMap(), relationKind, bindings);
}

} // namespace facts::commands::match
