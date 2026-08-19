#include "ast/extractors/Type.h"

#include "storage/FactStore.h"

#include <clang/AST/Decl.h>
#include <clang/AST/DeclTemplate.h>
#include <clang/AST/Type.h>
#include <clang/AST/TypeVisitor.h>
#include <clang/Basic/Version.h>
#include <clang/Index/USRGeneration.h>
#include <llvm/ADT/SmallString.h>

namespace facts {
namespace {

TypeResolutionError typeFailure(std::string target, std::string usr,
                                std::string detail,
                                bool targetMissing = false) {
  return TypeResolutionError{.target = std::move(target),
                             .usr = std::move(usr),
                             .detail = std::move(detail),
                             .targetMissing = targetMissing};
}

class TypeSymbolVisitor final
    : public clang::TypeVisitor<TypeSymbolVisitor, TypeResult> {
public:
  explicit TypeSymbolVisitor(FactStore &store) : store_(store) {}

  TypeResult VisitBuiltinType(const clang::BuiltinType *type) {
    return SymbolId{builtinFileId,
                    static_cast<std::uint32_t>(type->getKind()) + 1};
  }

  TypeResult VisitPointerType(const clang::PointerType *type) {
    return Visit(type->getPointeeType().getTypePtr());
  }

  TypeResult VisitReferenceType(const clang::ReferenceType *type) {
    return Visit(type->getPointeeType().getTypePtr());
  }

  TypeResult VisitArrayType(const clang::ArrayType *type) {
    return Visit(type->getElementType().getTypePtr());
  }

  TypeResult VisitAdjustedType(const clang::AdjustedType *type) {
    return Visit(type->getOriginalType().getTypePtr());
  }

  TypeResult VisitTypedefType(const clang::TypedefType *type) {
    return declarationId(type->getDecl());
  }

  TypeResult VisitTagType(const clang::TagType *type) {
    return declarationId(type->getDecl());
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
    return declarationId(type->getTemplateName().getAsTemplateDecl());
  }

  TypeResult VisitType(const clang::Type *) {
    return std::unexpected(typeFailure("<unsupported type>", "<unavailable>",
                                       "type is not supported"));
  }

private:
  TypeResult declarationId(const clang::NamedDecl *declaration) {
    if (declaration == nullptr) {
      return std::unexpected(typeFailure("<unavailable>", "<unavailable>",
                                         "type declaration is unavailable"));
    }

    const auto target = declaration->getQualifiedNameAsString();
    llvm::SmallString<128> usr;
    if (clang::index::generateUSRForDecl(declaration, usr)) {
      return std::unexpected(
          typeFailure(target, "<unavailable>", "type USR is unavailable"));
    }

    const auto usrText = std::string{usr.data(), usr.size()};
    return store_.findId(usrText)
        .transform_error([&](std::error_code error) {
          return typeFailure(target, usrText, error.message());
        })
        .and_then([&](std::optional<SymbolId> id) -> TypeResult {
          return id ? TypeResult{*id}
                    : std::unexpected(
                          typeFailure(target, usrText,
                                      "target symbol is not persisted", true));
        });
  }

  FactStore &store_;
};

} // namespace

TypeResult extractType(const clang::QualType &type, FactStore &store) {
  if (type.isNull()) {
    return std::unexpected(
        typeFailure("<unavailable>", "<unavailable>", "type is null"));
  }
  return TypeSymbolVisitor{store}.Visit(type.getTypePtr());
}

} // namespace facts
