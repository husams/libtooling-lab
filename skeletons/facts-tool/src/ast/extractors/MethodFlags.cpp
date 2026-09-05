#include "ast/extractors/MethodDecl.h"
#include <clang/AST/Attr.h>
#include <clang/AST/DeclCXX.h>

namespace facts {
namespace {
std::uint32_t flagWhen(SymbolBit flag, bool condition) {
  return condition ? bit(flag) : 0;
}

bool explicitMethod(const clang::CXXMethodDecl &node) {
  if (const auto *constructor =
          llvm::dyn_cast<clang::CXXConstructorDecl>(&node))
    return constructor->isExplicit();
  if (const auto *conversion = llvm::dyn_cast<clang::CXXConversionDecl>(&node))
    return conversion->isExplicit();
  return false;
}
} // namespace

ExtractionResult<Function> addMethodFlags(Function function,
                                          const clang::FunctionDecl &node) {
  const auto *method = llvm::dyn_cast<clang::CXXMethodDecl>(&node);
  if (!method)
    return function;
  function.flags = (function.flags & ~accessMask) |
                   static_cast<std::uint32_t>(method->getAccess());
  function.flags = (function.flags & ~(refQualifierMask << refQualifierShift)) |
                   (static_cast<std::uint32_t>(method->getRefQualifier())
                    << refQualifierShift);
  function.flags |=
      flagWhen(StaticBit, method->isStatic()) |
      flagWhen(ConstBit, method->isConst()) |
      flagWhen(VolatileBit, method->isVolatile()) |
      flagWhen(VirtualBit, method->isVirtual()) |
      flagWhen(PureBit, method->isPureVirtual()) |
      flagWhen(OverrideBit, method->size_overridden_methods() != 0) |
      flagWhen(FinalBit, method->hasAttr<clang::FinalAttr>()) |
      flagWhen(ExplicitBit, explicitMethod(*method));
  return function;
}
} // namespace facts
