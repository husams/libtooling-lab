#include "commands/match/RelationValidation.h"

#include "commands/match/RelationKinds.h"

#include <clang/AST/DeclCXX.h>
#include <clang/AST/DeclTemplate.h>

namespace facts::commands::match {
namespace {
bool supportedSymbol(const clang::NamedDecl &node) {
  return llvm::isa<clang::CXXRecordDecl, clang::FunctionDecl, clang::FieldDecl,
                   clang::VarDecl, clang::EnumDecl, clang::EnumConstantDecl,
                   clang::ClassTemplateDecl, clang::FunctionTemplateDecl,
                   clang::VarTemplateDecl>(node);
}

bool containsSource(const clang::NamedDecl &node) {
  return llvm::isa<clang::CXXRecordDecl, clang::EnumDecl, clang::TemplateDecl>(
      node);
}

bool usesSource(const clang::NamedDecl &node) {
  return llvm::isa<clang::FunctionDecl, clang::VarDecl>(node);
}
} // namespace

std::expected<void, std::string>
validateEndpoints(RelationKind kind, const clang::NamedDecl &source,
                  const clang::NamedDecl &target) {
  const auto records = llvm::isa<clang::CXXRecordDecl>(source) &&
                       llvm::isa<clang::CXXRecordDecl>(target);
  const auto methods = llvm::isa<clang::CXXMethodDecl>(source) &&
                       llvm::isa<clang::CXXMethodDecl>(target);
  bool valid = false;
  switch (kind) {
  case RelationKind::Inherits:
    valid = records;
    break;
  case RelationKind::Contains:
    valid = containsSource(source) && supportedSymbol(target);
    break;
  case RelationKind::Overrides:
    valid = methods;
    break;
  case RelationKind::Uses:
    valid = usesSource(source) && supportedSymbol(target);
    break;
  case RelationKind::FieldOf:
    valid = llvm::isa<clang::FieldDecl>(source) &&
            llvm::isa<clang::CXXRecordDecl>(target);
    break;
  case RelationKind::MethodOf:
    valid = llvm::isa<clang::CXXMethodDecl>(source) &&
            llvm::isa<clang::CXXRecordDecl>(target);
    break;
  default:
    valid = false;
  }
  if (!valid)
    return std::unexpected(std::string{relationName(kind)} +
                           " has incompatible source/target declarations");
  return {};
}

} // namespace facts::commands::match
