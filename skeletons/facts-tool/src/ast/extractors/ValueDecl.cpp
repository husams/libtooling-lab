#include "ast/extractors/ValueDecl.h"

#include "ast/extractors/NamedDecl.h"
#include "ast/extractors/Type.h"
#include "model/Relation.h"
#include "storage/FactStore.h"

#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <llvm/Support/Casting.h>

#include <array>
#include <optional>

namespace facts {
namespace {

IndexingResult storeFieldOwnerRelation(const clang::ValueDecl &node,
                                       SymbolId value, FactStore &store) {
  const auto *owner =
      llvm::dyn_cast<clang::CXXRecordDecl>(node.getDeclContext());
  if (owner == nullptr) {
    return {};
  }

  const auto source = node.getQualifiedNameAsString();
  const auto target = owner->getQualifiedNameAsString();
  const auto failure = [&](std::string_view usr, std::string_view detail) {
    return relationFailure("field_of", "source", source, "target", target, usr,
                           detail);
  };
  return extractUsr(*owner)
      .transform_error([&](ExtractionError) {
        return failure("<unavailable>", "owner USR is unavailable");
      })
      .and_then([&](std::string usr) -> IndexingResult {
        return store.findId(usr)
            .transform_error([&](std::error_code error) {
              return failure(usr, error.message());
            })
            .and_then([&](std::optional<SymbolId> ownerId) -> IndexingResult {
              if (!ownerId) {
                return std::unexpected(
                    failure(usr, "target symbol is not persisted"));
              }
              const std::array relations{Relation{
                  .source = value,
                  .destination = *ownerId,
                  .kind = RelationKind::FieldOf,
              }};
              return store.addRelations(relations).transform_error(
                  [&](std::error_code error) {
                    return failure(usr, error.message());
                  });
            });
      });
}

IndexingResult storeTypeRelation(const clang::ValueDecl &node, SymbolId value,
                                 const clang::SourceManager &sourceManager,
                                 FileManager &files, FactStore &store) {
  const auto source = node.getQualifiedNameAsString();
  const auto target = node.getType().getAsString();
  const auto failure = [&](std::string_view detail) {
    return relationFailure("of_type", "source", source, "target", target,
                           "<unavailable>", detail);
  };
  return extractType(node.getType(), sourceManager, files, store)
      .transform_error([&](TypeResolutionError error) {
        return relationFailure("of_type", "source", source, "target",
                               error.target, error.usr, error.detail);
      })
      .and_then([value, &store, &failure](SymbolId type) -> IndexingResult {
        if (type.file == builtinFileId) {
          return {};
        }
        const std::array relations{Relation{
            .source = value,
            .destination = type,
            .kind = RelationKind::OfType,
        }};
        return store.addRelations(relations).transform_error(
            [&](std::error_code error) { return failure(error.message()); });
      });
}

} // namespace

IndexingResult storeValueRelations(const clang::ValueDecl &node, SymbolId value,
                                   const clang::SourceManager &sourceManager,
                                   FileManager &files, FactStore &store) {
  return storeFieldOwnerRelation(node, value, store).and_then([&] {
    return storeTypeRelation(node, value, sourceManager, files, store);
  });
}

} // namespace facts
