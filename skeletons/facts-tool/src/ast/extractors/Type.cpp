#include "ast/extractors/Type.h"

#include "ast/extractors/NamedDecl.h"
#include "ast/extractors/TargetResolution.h"
#include "storage/FactStore.h"

#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/DeclTemplate.h>
#include <clang/AST/Type.h>
#include <clang/AST/TypeVisitor.h>
#include <clang/Basic/SourceManager.h>
#include <clang/Basic/Version.h>
#include <clang/Index/USRGeneration.h>
#include <llvm/ADT/SmallString.h>

namespace facts {
namespace {

TypeResolutionError typeFailure(std::string target, std::string usr,
                                std::string detail) {
  return TypeResolutionError{.target = std::move(target),
                             .usr = std::move(usr),
                             .detail = std::move(detail)};
}

TypeResult resolved(SymbolId id) { return std::optional<SymbolId>{id}; }

class TypeSymbolVisitor final
    : public clang::TypeVisitor<TypeSymbolVisitor, TypeResult> {
public:
  TypeSymbolVisitor(const clang::SourceManager &sourceManager,
                    FileManager &files, FactStore &store)
      : sourceManager_(sourceManager), files_(files), store_(store) {}

  TypeResult VisitBuiltinType(const clang::BuiltinType *type) {
    return resolved(SymbolId{builtinFileId,
                             static_cast<std::uint32_t>(type->getKind()) + 1});
  }

  TypeResult VisitPointerType(const clang::PointerType *type) {
    return Visit(type->getPointeeType().getTypePtr());
  }

  TypeResult VisitReferenceType(const clang::ReferenceType *type) {
    return Visit(type->getPointeeType().getTypePtr());
  }

  TypeResult VisitMemberPointerType(const clang::MemberPointerType *type) {
    return Visit(type->getPointeeType().getTypePtr());
  }

  TypeResult VisitBlockPointerType(const clang::BlockPointerType *type) {
    return Visit(type->getPointeeType().getTypePtr());
  }

  TypeResult VisitArrayType(const clang::ArrayType *type) {
    return Visit(type->getElementType().getTypePtr());
  }

  TypeResult VisitComplexType(const clang::ComplexType *type) {
    return Visit(type->getElementType().getTypePtr());
  }

  TypeResult VisitAtomicType(const clang::AtomicType *type) {
    return Visit(type->getValueType().getTypePtr());
  }

  TypeResult VisitVectorType(const clang::VectorType *type) {
    return Visit(type->getElementType().getTypePtr());
  }

  TypeResult VisitAdjustedType(const clang::AdjustedType *type) {
    return Visit(type->getOriginalType().getTypePtr());
  }

  TypeResult VisitParenType(const clang::ParenType *type) {
    return Visit(type->getInnerType().getTypePtr());
  }

  TypeResult VisitAutoType(const clang::AutoType *type) {
    const auto deduced = type->getDeducedType();
    return deduced.isNull() ? std::unexpected(typeFailure(
                                  "<undeduced auto>", "<unavailable>",
                                  "auto type does not have a deduced type"))
                            : Visit(deduced.getTypePtr());
  }

  TypeResult VisitFunctionType(const clang::FunctionType *type) {
    return Visit(type->getReturnType().getTypePtr());
  }

  TypeResult VisitTypedefType(const clang::TypedefType *type) {
    return declarationId(type->getDecl());
  }

  // A using-declaration brings a name in from elsewhere — libstdc++'s <cstdint>
  // writes "using ::uint16_t;" where libc++ writes a typedef — and the fact
  // belongs to what was brought in, not to the using-declaration. The shadow
  // declaration is reached by a different accessor before LLVM 22.
  TypeResult VisitUsingType(const clang::UsingType *type) {
#if CLANG_VERSION_MAJOR >= 22
    return declarationId(type->getDecl()->getTargetDecl());
#else
    return declarationId(type->getFoundDecl()->getTargetDecl());
#endif
  }

  TypeResult VisitTagType(const clang::TagType *type) {
    return declarationId(type->getDecl());
  }

  TypeResult
  VisitTemplateTypeParmType(const clang::TemplateTypeParmType *type) {
    return declarationId(type->getDecl());
  }

  TypeResult
  VisitSubstTemplateTypeParmType(const clang::SubstTemplateTypeParmType *type) {
    const auto replacement = type->getReplacementType();
    const auto *parameter =
        llvm::dyn_cast<clang::TemplateTypeParmType>(replacement.getTypePtr());
    return parameter != nullptr && parameter->getDecl() == nullptr
               ? declarationId(type->getReplacedParameter())
               : Visit(replacement.getTypePtr());
  }

#if CLANG_VERSION_MAJOR < 22
  // Before LLVM 22 a type written with a tag keyword or a nested-name
  // qualifier arrives wrapped in ElaboratedType sugar. Unwrap it — the named
  // type underneath still keeps any typedef the source wrote.
  TypeResult VisitElaboratedType(const clang::ElaboratedType *type) {
    return Visit(type->getNamedType().getTypePtr());
  }
#endif

  TypeResult VisitTemplateSpecializationType(
      const clang::TemplateSpecializationType *type) {
    const auto *declaration = type->getTemplateName().getAsTemplateDecl();
    return declaration == nullptr && type->isDependentType()
               ? TypeResult{std::optional<SymbolId>{}}
               : declarationId(declaration);
  }

  TypeResult VisitPackExpansionType(const clang::PackExpansionType *type) {
    return Visit(type->getPattern().getTypePtr());
  }

  TypeResult VisitDecltypeType(const clang::DecltypeType *type) {
    return Visit(type->getUnderlyingType().getTypePtr());
  }

  TypeResult VisitTypeOfExprType(const clang::TypeOfExprType *type) {
    return type->isDependentType()
               ? TypeResult{std::optional<SymbolId>{}}
               : Visit(type->getUnderlyingExpr()->getType().getTypePtr());
  }

  TypeResult VisitDependentNameType(const clang::DependentNameType *) {
    return std::optional<SymbolId>{};
  }

#if CLANG_VERSION_MAJOR < 22
  TypeResult VisitDependentTemplateSpecializationType(
      const clang::DependentTemplateSpecializationType *) {
    return std::optional<SymbolId>{};
  }
#endif

  TypeResult VisitType(const clang::Type *type) {
    if (type->isDependentType()) {
      return std::optional<SymbolId>{};
    }
    const auto canonical = type->getCanonicalTypeInternal();
    return canonical.getTypePtr() == type
               ? TypeResult{std::optional<SymbolId>{}}
               : Visit(canonical.getTypePtr());
  }

private:
  TypeResult declarationId(const clang::NamedDecl *declaration) {
    if (declaration == nullptr) {
      return std::unexpected(typeFailure("<unavailable>", "<unavailable>",
                                         "type declaration is unavailable"));
    }

    const auto target = declaration->getQualifiedNameAsString();
    // Through extractUsr, not generateUSRForDecl: a lambda's USR carries a
    // discriminator this lookup has to match.
    const auto usr = extractUsr(*declaration);
    if (!usr) {
      return std::unexpected(
          typeFailure(target, "<unavailable>", "type USR is unavailable"));
    }

    const auto usrText = *usr;
    return store_.findId(usrText)
        .transform_error([&](std::error_code error) {
          return typeFailure(target, usrText, error.message());
        })
        .and_then([&](std::optional<SymbolId> id) -> TypeResult {
          if (id) {
            return resolved(*id);
          }
          return findOrStoreSymbolTarget(*declaration, sourceManager_, files_,
                                         store_, usrText)
              .transform(
                  [](SymbolId id) { return std::optional<SymbolId>{id}; })
              .transform_error([&](std::error_code error) {
                return typeFailure(target, usrText, error.message());
              });
        });
  }

  const clang::SourceManager &sourceManager_;
  FileManager &files_;
  FactStore &store_;
};

} // namespace

TypeResult extractType(const clang::QualType &type,
                       const clang::SourceManager &sourceManager,
                       FileManager &files, FactStore &store) {
  if (type.isNull()) {
    return std::unexpected(
        typeFailure("<unavailable>", "<unavailable>", "type is null"));
  }
  return TypeSymbolVisitor{sourceManager, files, store}.Visit(
      type.getTypePtr());
}

} // namespace facts
