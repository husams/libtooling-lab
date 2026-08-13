#include "ast/extractors/Type.h"

#include "storage/FactStore.h"

#include <clang/AST/Decl.h>
#include <clang/AST/DeclTemplate.h>
#include <clang/AST/Type.h>
#include <clang/AST/TypeVisitor.h>
#include <clang/Index/USRGeneration.h>
#include <llvm/ADT/SmallString.h>

namespace facts {
namespace {

using TypeResult = std::expected<SymbolId, std::error_code>;

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

  TypeResult VisitTemplateSpecializationType(
      const clang::TemplateSpecializationType *type) {
    return declarationId(type->getTemplateName().getAsTemplateDecl());
  }

  TypeResult VisitType(const clang::Type *) {
    return std::unexpected(
        std::make_error_code(std::errc::operation_not_supported));
  }

private:
  TypeResult declarationId(const clang::NamedDecl *declaration) {
    if (declaration == nullptr) {
      return std::unexpected(std::make_error_code(std::errc::invalid_argument));
    }

    llvm::SmallString<128> usr;
    if (clang::index::generateUSRForDecl(declaration, usr)) {
      return std::unexpected(std::make_error_code(std::errc::invalid_argument));
    }

    return store_.findId(std::string_view{usr.data(), usr.size()})
        .and_then([](std::optional<SymbolId> id) -> TypeResult {
          return id ? TypeResult{*id}
                    : std::unexpected(std::make_error_code(
                          std::errc::no_such_file_or_directory));
        });
  }

  FactStore &store_;
};

} // namespace

TypeResult extractType(const clang::QualType &type, FactStore &store) {
  if (type.isNull()) {
    return std::unexpected(std::make_error_code(std::errc::invalid_argument));
  }
  return TypeSymbolVisitor{store}.Visit(type.getTypePtr());
}

} // namespace facts
