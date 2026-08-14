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
         llvm::isa<clang::FunctionDecl>(decl) ||
         llvm::isa<clang::ParmVarDecl>(decl) ||
         llvm::isa<clang::TemplateDecl>(decl) ||
         llvm::isa<clang::TemplateTypeParmDecl, clang::NonTypeTemplateParmDecl,
                   clang::TemplateTemplateParmDecl>(decl);
}

using AliasFacts = std::pair<SymbolId, std::vector<TemplateArgument>>;

std::expected<AliasFacts, std::error_code>
extractAliasFacts(clang::TypedefNameDecl &node, FactStore &store) {
  return extractAliasTarget(node, store).and_then([&](SymbolId target) {
    return extractAliasTemplateArguments(node, store)
        .transform([target](std::vector<TemplateArgument> arguments) {
          return AliasFacts{target, std::move(arguments)};
        })
        .transform_error([](ExtractionError) {
          return std::make_error_code(std::errc::invalid_argument);
        });
  });
}

void collectAlias(clang::TypedefNameDecl &node, clang::ASTContext &context,
                  FileManager &files, FactStore &store) {
  auto facts = extractAliasFacts(node, store);
  if (!facts) {
    return;
  }

  auto [target, arguments] = std::move(*facts);
  const auto stored = [&store, target,
                       arguments = std::move(arguments)](SymbolId source) {
    const std::array relations{Relation{
        .source = source,
        .destination = target,
        .kind = RelationKind::AliasOf,
    }};
    return store.addRelations(relations).and_then(
        [&]() { return store.addTemplateArguments(source, arguments); });
  };

  storeExtracted(node,
                 extractSymbol<Symbol>(static_cast<clang::NamedDecl &>(node),
                                       context.getSourceManager()),
                 context, files, store, stored);
}

} // namespace

void collectSymbol(clang::NamedDecl &node, clang::ASTContext &context,
                   FileManager &files, FactStore &store) {
  if (hasSpecializedExtractor(node)) {
    return;
  }

  if (auto *alias = llvm::dyn_cast<clang::TypedefNameDecl>(&node)) {
    collectAlias(*alias, context, files, store);
    return;
  }

  storeExtracted(node,
                 extractSymbol<Symbol>(node, context.getSourceManager())
                     .transform([](Symbol symbol) {
                       return classifySymbol(std::move(symbol));
                     }),
                 context, files, store);
}

} // namespace facts
