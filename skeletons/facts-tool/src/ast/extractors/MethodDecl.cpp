#include "ast/extractors/MethodDecl.h"

#include "ast/extractors/NamedDecl.h"
#include "ast/extractors/TargetResolution.h"
#include "model/Relation.h"
#include "storage/FactStore.h"

#include <clang/AST/DeclCXX.h>
#include <llvm/Support/Casting.h>

#include <array>
#include <optional>
#include <string>

namespace facts {
namespace {

const clang::CXXMethodDecl *supportedMethod(const clang::FunctionDecl &node) {
  const auto *method = llvm::dyn_cast<clang::CXXMethodDecl>(&node);
  if (!method || llvm::isa<clang::CXXConstructorDecl, clang::CXXDestructorDecl,
                           clang::CXXConversionDecl>(method)) {
    return nullptr;
  }
  return method;
}

} // namespace

IndexingResult storeMethodRelation(const clang::FunctionDecl &node,
                                   SymbolId function,
                                   const clang::SourceManager &sourceManager,
                                   FileManager &files, FactStore &store) {
  const auto *method = supportedMethod(node);
  if (!method) {
    return {};
  }

  const auto source = node.getQualifiedNameAsString();
  const auto target = method->getParent()->getQualifiedNameAsString();
  const auto failure = [&](std::string_view usr, std::string_view detail) {
    return relationFailure("method_of", "source", source, "target", target, usr,
                           detail);
  };
  const auto invalidUsr = [&](ExtractionError) {
    return failure("<unavailable>", "owner USR is unavailable");
  };
  const auto findAndStore = [&](std::string usr) -> IndexingResult {
    return findOrStoreSymbolTarget(*method->getParent(), sourceManager, files,
                                   store, usr)
        .transform_error([&](std::error_code error) {
          return failure(usr, error.message());
        })
        .and_then([&](SymbolId owner) -> IndexingResult {
          const std::array relations{Relation{
              .source = function,
              .destination = owner,
              .kind = RelationKind::MethodOf,
          }};
          return store.addRelations(relations).transform_error(
              [&](std::error_code error) {
                return failure(usr, error.message());
              });
        });
  };

  return extractUsr(*method->getParent())
      .transform_error(invalidUsr)
      .and_then(findAndStore);
}

} // namespace facts
