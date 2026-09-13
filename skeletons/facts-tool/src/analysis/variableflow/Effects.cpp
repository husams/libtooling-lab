#include "analysis/variableflow/Traversal.h"

#include <ranges>

namespace facts::variableflow::detail {

void Traversal::linkMemoryInput(Summary &caller, Summary &callee,
                                const Call &call, unsigned index) {
  const auto *parameter = callee.function->decl->getParamDecl(index);
  const auto entry = callee.memoryParameters.find(parameter);
  const auto &argument = call.arguments[index];
  if (entry == callee.memoryParameters.end() || argument.variable == nullptr)
    return;
  const auto memoryRead =
      std::ranges::any_of(callee.variables[parameter], [&](auto id) {
        const auto &node =
            builder.graph.nodes.at(static_cast<std::size_t>(id - 1));
        return node.variableUsr.ends_with("#pointee") &&
               (node.kind == "read" || node.kind == "update");
      });
  if (!memoryRead)
    return;
  const auto block = blockFor(call.expression, *caller.function, caller.blocks);
  const auto id = builder.node("read", *caller.function, argument.variable,
                               call.expression->getExprLoc(), block,
                               caller.depth, {}, !argument.address);
  builder.event(id, *caller.function, argument.variable, Access::Read, block,
                orderFor(call.expression, *caller.function, caller.blocks),
                !argument.address);
  if (owners.emplace(id, &caller).second) {
    caller.nodes.push_back(id);
    caller.variables[argument.variable].push_back(id);
    caller.variableOf[id] = argument.variable;
  }
  builder.edge(id, entry->second, "argument-pointee", call.node);
  builder.edge(id, call.node, "argument", call.node);
  activate(id);
  activate(entry->second);
}

std::int64_t Traversal::callerEffect(Summary &caller, const Call &call,
                                     unsigned index, bool memory) {
  const auto key = std::make_tuple(call.node, index, memory);
  if (const auto found = effectNodes.find(key); found != effectNodes.end())
    return found->second;
  const auto *variable = call.arguments[index].variable;
  const auto block = blockFor(call.expression, *caller.function, caller.blocks);
  const auto id = builder.node("write", *caller.function, variable,
                               call.expression->getExprLoc(), block,
                               caller.depth, {}, memory);
  builder.event(id, *caller.function, variable, Access::MayWrite, block,
                orderFor(call.expression, *caller.function, caller.blocks),
                memory);
  effectNodes.emplace(key, id);
  caller.nodes.push_back(id);
  caller.variables[variable].push_back(id);
  caller.variableOf.emplace(id, variable);
  owners.emplace(id, &caller);
  const auto *parameter = llvm::dyn_cast<clang::ParmVarDecl>(variable);
  if (parameter != nullptr &&
      (parameter->getType()->isReferenceType() || memory))
    caller.effects[parameter].push_back(id);
  // The call is the event that may change the caller's storage, including when
  // the callee has a void return type.
  builder.edge(call.node, id, "effect", call.node);
  activate(id);
  return id;
}

void Traversal::linkEffects() {
  for (const auto &binding : bindings) {
    linkMemoryInput(*binding.caller, *binding.callee, *binding.call,
                    binding.argument);
    const auto *parameter =
        binding.callee->function->decl->getParamDecl(binding.argument);
    const auto found = binding.callee->effects.find(parameter);
    const auto &argument = binding.call->arguments[binding.argument];
    if (found == binding.callee->effects.end() || argument.variable == nullptr)
      continue;
    const auto sources = found->second;
    for (const auto source : sources) {
      if (!active.contains(source))
        continue;
      const auto occurrence =
          std::ranges::find(builder.graph.nodes, source, &Node::id);
      const auto memory = occurrence != builder.graph.nodes.end() &&
                          occurrence->variableUsr.ends_with("#pointee") &&
                          !argument.address;
      const auto target = callerEffect(*binding.caller, *binding.call,
                                       binding.argument, memory);
      builder.edge(source, target, "reference-effect", binding.call->node);
    }
  }
}

} // namespace facts::variableflow::detail
