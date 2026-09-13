#include "analysis/variableflow/Traversal.h"

#include <clang/AST/DeclCXX.h>
#include <clang/AST/ExprCXX.h>

namespace facts::variableflow::detail {
namespace {

bool dynamicDispatch(const clang::CallExpr &call) {
  const auto *method =
      llvm::dyn_cast_or_null<clang::CXXMethodDecl>(call.getDirectCallee());
  if (method == nullptr || !method->isVirtual())
    return false;
  const auto *member =
      llvm::dyn_cast<clang::MemberExpr>(strip(call.getCallee()));
  return member == nullptr || !member->hasQualifier();
}

const char *argumentKind(const clang::ParmVarDecl &parameter) {
  if (parameter.getType()->isReferenceType())
    return "argument-ref";
  return parameter.getType()->isPointerType() ? "argument-pointer"
                                              : "argument-copy";
}

bool writableIndirect(clang::QualType type) {
  if (!type->isReferenceType() && !type->isPointerType())
    return false;
  return !type->getPointeeType().isConstQualified();
}

void unknownEffects(Traversal &traversal, Summary &caller, const Call &call) {
  const auto *callee = call.expression->getDirectCallee();
  for (unsigned index = 0; index < call.arguments.size(); ++index) {
    const auto &argument = call.arguments[index];
    if (argument.variable == nullptr)
      continue;
    const auto type = callee != nullptr && index < callee->getNumParams()
                          ? callee->getParamDecl(index)->getType()
                          : argument.variable->getType();
    if (!writableIndirect(type) && !(callee == nullptr && argument.address))
      continue;
    const auto memory = type->isPointerType() && !argument.address;
    const auto write = traversal.callerEffect(caller, call, index, memory);
    traversal.builder.edge(call.node, write, "unknown-effect", call.node);
  }
}

} // namespace

void Traversal::expandCall(Summary &caller, const Call &call) {
  const auto indexed = parsed.callTargets.find(call.expression);
  const auto *decl = indexed == parsed.callTargets.end()
                         ? call.expression->getDirectCallee()
                         : indexed->second;
  const auto usr = decl ? usrFor(*decl, caller.function->tu) : std::string{};
  const auto target = parsed.byUsr.find(usr);
  const auto reason =
      decl == nullptr                                          ? "indirect"
      : dynamicDispatch(*call.expression)                      ? "virtual"
      : target == parsed.byUsr.end() || target->second.empty() ? "external"
      : request.maxDepth && caller.depth >= *request.maxDepth  ? "depth-limit"
                                                               : "";
  if (*reason != '\0') {
    builder.boundary(call.node, reason,
                     usr.empty() ? "unresolved call target" : usr,
                     caller.depth);
    unknownEffects(*this, caller, call);
    return;
  }

  auto &callee = summary(*target->second.front(), caller.depth + 1);
  builder.edge(call.node, callee.owner, "call", call.node);
  activate(callee.owner);
  for (unsigned index = 0; index < call.arguments.size(); ++index) {
    if (index >= callee.function->decl->getNumParams()) {
      builder.boundary(call.node, "variadic", "unmodeled variadic argument",
                       caller.depth);
      continue;
    }
    const auto *parameter = callee.function->decl->getParamDecl(index);
    const auto parameterId = callee.parameters.at(parameter);
    for (const auto source : call.arguments[index].sources)
      builder.edge(source, parameterId, argumentKind(*parameter), call.node);
    activate(parameterId);
    linkMemoryInput(caller, callee, call, index);
    bindings.push_back(Binding{&caller, &callee, &call, index});
  }
  for (const auto id : callee.returns) {
    builder.edge(id, call.node, "return", call.node);
    activate(id);
  }
}

} // namespace facts::variableflow::detail
