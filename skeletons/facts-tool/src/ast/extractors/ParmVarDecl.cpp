#include "ast/extractors/ParmVarDecl.h"

#include "ast/extractors/Location.h"

#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclTemplate.h"
#include "clang/AST/Type.h"
#include "llvm/Support/Casting.h"

namespace facts {
namespace {

std::uint8_t flagWhen(ParameterBit flag, bool condition) {
  return condition ? bit(flag) : 0;
}

bool isConstQualified(clang::QualType type) {
  if (type.isConstQualified()) {
    return true;
  }

  if (!type->isPointerType() && !type->isReferenceType()) {
    return false;
  }

  return type->getPointeeType().isConstQualified();
}

bool isForwardingReference(const clang::ParmVarDecl &node,
                           clang::QualType type) {
  const auto *reference = type->getAs<clang::RValueReferenceType>();
  if (reference == nullptr) {
    return false;
  }

  const auto referredType = reference->getPointeeType();
  const auto *typeParameter =
      referredType->getAs<clang::TemplateTypeParmType>();
  if (referredType.getCVRQualifiers() != 0 || typeParameter == nullptr) {
    return false;
  }

  const auto *function =
      llvm::dyn_cast<clang::FunctionDecl>(node.getDeclContext());
  const auto *functionTemplate =
      function == nullptr ? nullptr : function->getDescribedFunctionTemplate();
  if (functionTemplate == nullptr) {
    return false;
  }

  return typeParameter->getDepth() ==
         functionTemplate->getTemplateParameters()->getDepth();
}

std::uint8_t extractParameterFlags(const clang::ParmVarDecl &node) {
  const auto type = node.getType();

  return flagWhen(ParameterBit::PointerBit, type->isPointerType()) |
         flagWhen(ParameterBit::LValueReferenceBit,
                  type->isLValueReferenceType()) |
         flagWhen(ParameterBit::RValueReferenceBit,
                  type->isRValueReferenceType()) |
         flagWhen(ParameterBit::ForwardingReferenceBit,
                  isForwardingReference(node, type)) |
         flagWhen(ParameterBit::ConstBit, isConstQualified(type)) |
         flagWhen(ParameterBit::PackBit, node.isParameterPack());
}

} // namespace

ExtractionResult<Parameter>
extractParameter(const clang::ParmVarDecl &node,
                 const clang::SourceManager &sourceManager) {
  const auto toParameter = [&](Location location) {
    const auto withRegion =
        [&, location](Region region) -> ExtractionResult<Parameter> {
      return Parameter{
          .name = node.getNameAsString(),
          .type = node.getType().getAsString(),
          .loc = location,
          .region = region,
          .flags = extractParameterFlags(node),
          .hasDefault = node.hasDefaultArg(),
      };
    };

    return extractRegion(sourceManager, node.getASTContext().getLangOpts(),
                         node.getSourceRange()) |
           withRegion;
  };

  return extractLocation(sourceManager, node.getLocation()) | toParameter;
}

} // namespace facts
