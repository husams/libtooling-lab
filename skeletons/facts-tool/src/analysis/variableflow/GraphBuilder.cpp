#include "analysis/variableflow/FlowSupport.h"

#include <clang/AST/Decl.h>

#include <functional>

namespace facts::variableflow::detail {
namespace {

template <typename Value>
void mix(std::size_t &seed, const Value &value) noexcept {
  seed ^= std::hash<Value>{}(value) + static_cast<std::size_t>(0x9e3779b9) +
          (seed << 6) + (seed >> 2);
}

std::string variableIdentity(const clang::VarDecl *variable,
                             std::string_view tu, bool memory) {
  if (variable == nullptr)
    return {};
  auto result = usrFor(*variable, tu);
  if (memory)
    result += "#pointee";
  return result;
}

} // namespace

std::size_t NodeKeyHash::operator()(const NodeKey &key) const noexcept {
  std::size_t seed = 0;
  mix(seed, key.functionUsr);
  mix(seed, key.kind);
  mix(seed, key.variableUsr);
  mix(seed, key.file);
  mix(seed, key.offset);
  mix(seed, key.block);
  mix(seed, key.name);
  mix(seed, key.memory);
  return seed;
}

std::size_t EdgeKeyHash::operator()(const EdgeKey &key) const noexcept {
  std::size_t seed = 0;
  mix(seed, key.source);
  mix(seed, key.target);
  mix(seed, key.kind);
  mix(seed, key.callsite);
  return seed;
}

std::size_t BoundaryKeyHash::operator()(const BoundaryKey &key) const noexcept {
  std::size_t seed = 0;
  mix(seed, key.node);
  mix(seed, key.reason);
  mix(seed, key.detail);
  mix(seed, key.depth);
  return seed;
}

std::int64_t Builder::node(std::string kind, const Function &function,
                           const clang::VarDecl *variable,
                           clang::SourceLocation loc, int block, unsigned depth,
                           const std::string &name, bool memory) {
  const auto &manager = function.context->getSourceManager();
  const auto location = locationOf(manager, loc);
  const auto variableUsr = variableIdentity(variable, function.tu, memory);
  const auto nodeName = name.empty() ? (variable ? variable->getNameAsString()
                                                 : functionName(*function.decl))
                                     : name;
  const NodeKey key{function.usr,
                    kind,
                    variableUsr,
                    location.file,
                    location.offset,
                    block,
                    kind == "unsupported" ? "" : nodeName,
                    memory};
  if (const auto found = nodeIds.find(key); found != nodeIds.end())
    return found->second;

  const auto id = nextId++;
  graph.nodes.push_back(Node{
      .id = id,
      .kind = std::move(kind),
      .functionUsr = function.usr,
      .variableUsr = variableUsr,
      .name = nodeName,
      .type = variable ? (memory && (variable->getType()->isPointerType() ||
                                     variable->getType()->isReferenceType())
                              ? variable->getType()->getPointeeType()
                              : variable->getType())
                             .getAsString()
                       : std::string{},
      .location = location,
      .block = block,
      .depth = depth});
  nodeIds.emplace(key, id);
  return id;
}

void Builder::edge(std::int64_t source, std::int64_t target, std::string kind,
                   std::int64_t callsite) {
  const EdgeKey key{source, target, kind, callsite};
  if (!edges.insert(key).second)
    return;
  graph.edges.push_back(Edge{source, target, std::move(kind), callsite});
}

void Builder::boundary(std::int64_t nodeId, std::string reason,
                       std::string detail, unsigned depth) {
  const BoundaryKey key{nodeId, reason, detail, depth};
  if (!boundaries.insert(key).second)
    return;
  graph.boundaries.push_back(
      Boundary{nodeId, std::move(reason), std::move(detail), depth});
}

void Builder::event(std::int64_t id, const Function &function,
                    const clang::VarDecl *variable, Access kind, int block,
                    int statementOrder, bool memory) {
  if (!eventNodes.insert(id).second)
    return;
  if (block < 0 && variable && llvm::isa<clang::ParmVarDecl>(variable)) {
    const auto found = blockGraphs.find(function.usr);
    if (found != blockGraphs.end())
      block = found->second.entry;
  }
  events.push_back(AccessEvent{
      id, function.usr, variableIdentity(variable, function.tu, memory), kind,
      block, statementOrder, nextEvent++, memory});
}

} // namespace facts::variableflow::detail
