#include "ast/extractors/ReturnType.h"

#include "ast/extractors/Type.h"
#include "model/ReturnType.h"
#include "storage/FactStore.h"

#include <clang/AST/ASTContext.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/PrettyPrinter.h>

namespace facts {
namespace {

std::string builtinName(SymbolId target, const clang::ASTContext &context) {
  if (target.file != builtinFileId)
    return {};
  switch (target.index) {
#define BUILTIN_TYPE(Id, SingletonId)                                          \
  case static_cast<std::uint32_t>(clang::BuiltinType::Id) + 1:                 \
    return clang::QualType(context.SingletonId)                                \
        .getAsString(context.getPrintingPolicy());
#include <clang/AST/BuiltinTypes.def>
  default:
    return {};
  }
}

bool hasReturnType(const clang::FunctionDecl &node) {
  // Constructors/destructors have no C++ return type. A template's undeduced
  // return is recorded when a concrete specialization becomes available.
  if (llvm::isa<clang::CXXConstructorDecl, clang::CXXDestructorDecl>(node) ||
      node.getReturnType().isNull())
    return false;
  const auto *deduced = node.getReturnType()->getContainedAutoType();
  return deduced == nullptr || !deduced->getDeducedType().isNull();
}

} // namespace

IndexingResult storeReturnType(const clang::FunctionDecl &node,
                               SymbolId callable, FileManager &files,
                               FactStore &store) {
  if (!hasReturnType(node))
    return {};
  const auto &context = node.getASTContext();
  auto policy = context.getPrintingPolicy();
  policy.SuppressTagKeyword = true;
  const auto spelling =
      node.getReturnType().getCanonicalType().getAsString(policy);
  const auto failure = [&](std::string_view detail) {
    return relationFailure("return_type", "source",
                           node.getQualifiedNameAsString(), "target", spelling,
                           "<unavailable>", detail);
  };
  return extractType(node.getReturnType(), context.getSourceManager(), files,
                     store)
      .transform_error([&](const TypeResolutionError &error) {
        return relationFailure("return_type", "source",
                               node.getQualifiedNameAsString(), "target",
                               error.target, error.usr, error.detail);
      })
      .and_then([&](std::optional<SymbolId> target) -> IndexingResult {
        if (!target)
          return {}; // Dependent types may have no declaration target yet.
        return store
            .saveReturnType(callable, ReturnType{*target, spelling,
                                                 builtinName(*target, context)})
            .transform_error([&](std::error_code error) {
              return failure(error.message());
            });
      });
}

} // namespace facts
