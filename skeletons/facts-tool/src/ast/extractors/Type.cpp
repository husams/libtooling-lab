#include "ast/extractors/Type.h"

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

class TypeSymbolVisitor final
    : public clang::TypeVisitor<TypeSymbolVisitor, TypeResult> {
public:
  TypeSymbolVisitor(const clang::SourceManager &sourceManager,
                    FileManager &files, FactStore &store)
      : sourceManager_(sourceManager), files_(files), store_(store) {}

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

#if CLANG_VERSION_MAJOR >= 22
  TypeResult VisitUsingType(const clang::UsingType *type) {
    return declarationId(type->getDecl()->getTargetDecl());
  }
#endif

  TypeResult VisitTagType(const clang::TagType *type) {
    return declarationId(type->getDecl());
  }

  TypeResult
  VisitTemplateTypeParmType(const clang::TemplateTypeParmType *type) {
    return declarationId(type->getDecl());
  }

  TypeResult
  VisitSubstTemplateTypeParmType(const clang::SubstTemplateTypeParmType *type) {
    return Visit(type->getReplacementType().getTypePtr());
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
          if (id) {
            return *id;
          }
          return findOrStoreSymbolTarget(*declaration, sourceManager_, files_,
                                         store_, usrText)
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
