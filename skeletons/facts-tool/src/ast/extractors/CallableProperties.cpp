#include "ast/extractors/CallableProperties.h"
#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/Type.h>

namespace facts {
namespace {
std::uint32_t flagWhen(SymbolBit flag, bool condition) {
  return condition ? bit(flag) : 0;
}

bool provenNothrow(const clang::FunctionDecl &node) {
  const auto *type = node.getType()->getAs<clang::FunctionProtoType>();
  if (!type)
    return false;
  // Inspect semantic results only: never force a dependent or lazy spec.
  switch (type->getExceptionSpecType()) {
  case clang::EST_DynamicNone:
  case clang::EST_NoThrow:
  case clang::EST_BasicNoexcept:
  case clang::EST_NoexceptTrue:
    return true;
  case clang::EST_Unevaluated:
    // A trivial special member performs no potentially throwing operation.
    // Written noexcept(false) takes the default branch, even when trivial.
    return node.isTrivial() && !node.isDependentContext();
  default:
    return false;
  }
}
} // namespace

ExtractionResult<Function>
addCallableProperties(Function function, const clang::FunctionDecl &node) {
  function.flags =
      (function.flags & ~(constexprMask << constexprShift)) |
      (static_cast<std::uint32_t>(node.getConstexprKind()) << constexprShift);
  function.flags |=
      flagWhen(NoexceptBit, provenNothrow(node)) |
      flagWhen(VariadicBit, node.isVariadic()) |
      flagWhen(InlineBit, node.isInlined()) |
      flagWhen(DeletedBit, node.isDeleted()) |
      flagWhen(DefaultedBit, node.isDefaulted()) |
      flagWhen(ImplicitBit, node.isImplicit()) |
      flagWhen(StaticBit, node.getStorageClass() == clang::SC_Static) |
      flagWhen(ExternStorageBit, node.getStorageClass() == clang::SC_Extern) |
      flagWhen(InternalLinkageBit,
               node.getFormalLinkage() == clang::Linkage::Internal);
  return function;
}
} // namespace facts
