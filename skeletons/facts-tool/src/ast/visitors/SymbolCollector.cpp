#include "ast/visitors/SymbolCollector.h"
#include "ast/StoreExtracted.h"

#include "ast/extractors/File.h"
#include "ast/extractors/FunctionDecl.h"
#include "ast/extractors/NamedDecl.h"
#include "ast/extractors/RecordDecl.h"
#include "model/AnySymbol.h"
#include "model/Relation.h"
#include "storage/FactStore.h"
#include "storage/FileManager.h"

#include <clang/AST/ASTContext.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/DeclTemplate.h>
#include <clang/Basic/SourceManager.h>
#include <llvm/Support/Casting.h>
#include <llvm/Support/raw_ostream.h>

#include <array>
#include <expected>
#include <system_error>
#include <utility>
#include <vector>

namespace facts {
namespace {

bool hasSpecializedExtractor(const clang::NamedDecl &decl) {
  return llvm::isa<clang::CXXRecordDecl>(decl) ||
         llvm::isa<clang::EnumDecl>(decl) ||
         llvm::isa<clang::EnumConstantDecl>(decl) ||
         llvm::isa<clang::FunctionDecl>(decl) ||
         llvm::isa<clang::ParmVarDecl>(decl) ||
         llvm::isa<clang::VarDecl>(decl) ||
         llvm::isa<clang::TemplateDecl>(decl) ||
         llvm::isa<clang::TemplateTypeParmDecl, clang::NonTypeTemplateParmDecl,
                   clang::TemplateTemplateParmDecl>(decl);
}

using AliasFacts = std::pair<SymbolId, std::vector<TemplateArgument>>;

std::expected<AliasFacts, IndexingError>
extractAliasFacts(clang::TypedefNameDecl &node,
                  const clang::SourceManager &sourceManager, FileManager &files,
                  FactStore &store) {
  const auto source = node.getQualifiedNameAsString();
  const auto target = node.getUnderlyingType().getAsString();
  return extractAliasTarget(node, sourceManager, files, store)
      .transform_error([&](TypeResolutionError error) {
        return relationFailure("alias_of", "source", source, "target",
                               error.target, error.usr, error.detail);
      })
      .and_then([&](SymbolId targetId) {
        return extractAliasTemplateArguments(node, sourceManager, files, store)
            .transform([targetId](std::vector<TemplateArgument> arguments) {
              return AliasFacts{targetId, std::move(arguments)};
            })
            .transform_error([&](ExtractionError error) {
              return relationFailure("alias_of", "source", source, "target",
                                     target, "<unavailable>",
                                     extractionErrorName(error));
            });
      });
}

IndexingResult collectAlias(clang::TypedefNameDecl &node,
                            clang::ASTContext &context, FileManager &files,
                            FactStore &store) {
  return withContext(
             extractAliasFacts(node, context.getSourceManager(), files, store),
             "cannot extract alias facts for '" +
                 node.getQualifiedNameAsString() + "'")
      .and_then([&](AliasFacts facts) {
        auto [target, arguments] = std::move(facts);
        const auto sourceName = node.getQualifiedNameAsString();
        const auto targetName = node.getUnderlyingType().getAsString();
        const auto stored = [&store, target, sourceName, targetName,
                             arguments =
                                 std::move(arguments)](SymbolId source) {
          if (target.file == builtinFileId) {
            return IndexingResult{};
          }
          const std::array relations{Relation{
              .source = source,
              .destination = target,
              .kind = RelationKind::AliasOf,
          }};
          return store.addRelations(relations)
              .transform_error([&](std::error_code error) {
                return relationFailure("alias_of", "source", sourceName,
                                       "target", targetName, "<unavailable>",
                                       error.message());
              })
              .and_then([&]() {
                return withContext(
                    store.addTemplateArguments(source, arguments),
                    "cannot persist alias template arguments for '" +
                        sourceName + "'");
              });
        };

        return storeExtracted(
            node,
            extractSymbol<Symbol>(static_cast<clang::NamedDecl &>(node),
                                  context.getSourceManager()),
            context, files, store, stored);
      });
}

} // namespace

IndexingResult collectSymbol(clang::NamedDecl &node, clang::ASTContext &context,
                             FileManager &files, FactStore &store) {
  if (hasSpecializedExtractor(node)) {
    return {};
  }

  if (auto *alias = llvm::dyn_cast<clang::TypedefNameDecl>(&node)) {
    return collectAlias(*alias, context, files, store);
  }

  return storeExtracted(node,
                        extractSymbol<Symbol>(node, context.getSourceManager())
                            .transform([](Symbol symbol) {
                              return classifySymbol(std::move(symbol));
                            }),
                        context, files, store);
}

} // namespace facts
