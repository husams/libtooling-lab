#include "ast/extractors/MethodDecl.h"

#include "ast/extractors/NamedDecl.h"
#include "model/Relation.h"
#include "storage/FactStore.h"

#include <clang/AST/DeclCXX.h>
#include <llvm/Support/Casting.h>

#include <array>
#include <optional>
#include <string>

namespace facts {
namespace {

std::uint32_t flagWhen(SymbolBit flag, bool condition) {
  return condition ? bit(flag) : 0;
}

const clang::CXXMethodDecl *supportedMethod(const clang::FunctionDecl &node) {
  const auto *method = llvm::dyn_cast<clang::CXXMethodDecl>(&node);
  if (!method || llvm::isa<clang::CXXConstructorDecl, clang::CXXDestructorDecl,
                           clang::CXXConversionDecl>(method)) {
    return nullptr;
  }
  return method;
}

} // namespace

ExtractionResult<Function> addMethodFlags(Function function,
                                          const clang::FunctionDecl &node) {
  const auto *method = supportedMethod(node);
  if (!method) {
    return function;
  }

  function.flags |=
      static_cast<std::uint32_t>(method->getAccess()) |
      flagWhen(VirtualBit, method->isVirtual()) |
      flagWhen(PureBit, method->isPureVirtual()) |
      flagWhen(OverrideBit, method->size_overridden_methods() != 0) |
      flagWhen(InlineBit, method->isInlined()) |
      flagWhen(DefaultedBit, method->isDefaulted()) |
      flagWhen(DeletedBit, method->isDeleted());
  return function;
}

std::expected<void, std::error_code>
storeMethodRelation(const clang::FunctionDecl &node, SymbolId function,
                    FactStore &store) {
  const auto *method = supportedMethod(node);
  if (!method) {
    return {};
  }

  const auto invalidUsr = [](ExtractionError) {
    return std::make_error_code(std::errc::invalid_argument);
  };
  const auto findOwner = [&store](std::string ownerUsr) {
    return store.findId(ownerUsr);
  };
  const auto storeRelation = [function, &store](std::optional<SymbolId> owner)
      -> std::expected<void, std::error_code> {
    if (!owner) {
      return std::unexpected(
          std::make_error_code(std::errc::no_such_file_or_directory));
    }

    const std::array relations{Relation{
        .source = function,
        .destination = *owner,
        .kind = RelationKind::MethodOf,
    }};
    return store.addRelations(relations);
  };

  return extractUsr(*method->getParent())
      .transform_error(invalidUsr)
      .and_then(findOwner)
      .and_then(storeRelation);
}

} // namespace facts
