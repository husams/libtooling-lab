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

std::expected<void, std::error_code>
storeFieldOwnerRelation(const clang::ValueDecl &node, SymbolId value,
                        FactStore &store) {
  const auto *owner =
      llvm::dyn_cast<clang::CXXRecordDecl>(node.getDeclContext());
  if (owner == nullptr) {
    return {};
  }

  return extractUsr(*owner)
      .transform_error([](ExtractionError) {
        return std::make_error_code(std::errc::invalid_argument);
      })
      .and_then([&store](std::string usr) { return store.findId(usr); })
      .and_then([value, &store](std::optional<SymbolId> ownerId)
                    -> std::expected<void, std::error_code> {
        if (!ownerId) {
          return std::unexpected(
              std::make_error_code(std::errc::no_such_file_or_directory));
        }
        const std::array relations{Relation{
            .source = value,
            .destination = *ownerId,
            .kind = RelationKind::FieldOf,
        }};
        return store.addRelations(relations);
      });
}

std::expected<void, std::error_code>
storeTypeRelation(const clang::ValueDecl &node, SymbolId value,
                  FactStore &store) {
  return extractType(node.getType(), store)
      .and_then([value, &store](
                    SymbolId type) -> std::expected<void, std::error_code> {
        if (type.file == builtinFileId) {
          return {};
        }
        const std::array relations{Relation{
            .source = value,
            .destination = type,
            .kind = RelationKind::OfType,
        }};
        return store.addRelations(relations);
      });
}

} // namespace

std::expected<void, std::error_code>
storeValueRelations(const clang::ValueDecl &node, SymbolId value,
                    FactStore &store) {
  return storeFieldOwnerRelation(node, value, store).and_then([&] {
    return storeTypeRelation(node, value, store);
  });
}

} // namespace facts
