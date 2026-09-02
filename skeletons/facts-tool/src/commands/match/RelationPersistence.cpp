#include "commands/match/RelationPersistence.h"

#include "ast/extractors/File.h"
#include "ast/extractors/Location.h"
#include "commands/match/RelationKinds.h"
#include "commands/match/RelationValidation.h"
#include "commands/match/SymbolDispatch.h"
#include "storage/FactStore.h"

#include <clang/AST/ASTContext.h>

#include <array>
#include <iostream>

namespace facts::commands::match {
namespace {
clang::SourceLocation siteLocation(const RelationMatch &match) {
  if (match.site)
    return match.site->getBeginLoc();
  if (match.declarationSite)
    return match.declarationSite->getLocation();
  return match.source.getLocation();
}

std::expected<void, std::string>
saveSite(Relation relation, clang::SourceLocation location,
         clang::ASTContext &context, FileManager &files, FactStore &store) {
  auto place = extractLocation(context.getSourceManager(), location);
  auto file = resolveFile(context.getSourceManager(), location, files);
  if (!place || !file)
    return std::unexpected("relation site has no persistable source location");
  const std::array relations{relation};
  const std::array sites{RelationSite{.source = relation.source,
                                      .destination = relation.destination,
                                      .kind = relation.kind,
                                      .file = *file,
                                      .location = *place}};
  return store.addRelationFacts(relations, sites)
      .transform_error([](std::error_code error) { return error.message(); });
}
} // namespace

std::expected<void, std::string> persistRelation(const RelationMatch &match,
                                                 clang::ASTContext &context,
                                                 FileManager &files,
                                                 FactStore &store) {
  return validateEndpoints(match.kind, match.source, match.target)
      .and_then(
          [&] { return persistSymbol(match.source, context, files, store); })
      .and_then([&](PersistedSymbol source) {
        return persistSymbol(match.target, context, files, store)
            .and_then([&](PersistedSymbol target) {
              if (match.kind == RelationKind::Overrides &&
                  match.declarationSite &&
                  match.declarationSite->getCanonicalDecl() !=
                      match.source.getCanonicalDecl())
                return std::expected<void, std::string>{std::unexpected(
                    "Overrides site must bind the source declaration")};
              const Relation relation{.source = source.id,
                                      .destination = target.id,
                                      .kind = match.kind};
              auto persisted = [&] {
                if (siteBacked(match.kind))
                  return saveSite(relation, siteLocation(match), context,
                                  files, store);
                const std::array relations{relation};
                return store.addRelations(relations).transform_error(
                    [](std::error_code error) { return error.message(); });
              }();
              return persisted.transform([&] {
                std::cout << "relation kind=" << relationName(match.kind)
                          << " source=" << source.name
                          << " target=" << target.name << '\n';
              });
            });
      });
}

} // namespace facts::commands::match
