#include "ast/extractors/OverrideRelation.h"

#include "ast/extractors/File.h"
#include "ast/extractors/Location.h"
#include "ast/extractors/RelationTarget.h"

#include <clang/AST/DeclCXX.h>

namespace facts {

ExtractionResult<std::vector<callgraph::OverrideFact>>
extractOverrideRelations(const clang::CXXMethodDecl &method,
                         const clang::SourceManager &sourceManager,
                         FileManager &files, FactStore &store) {
  std::vector<callgraph::OverrideFact> facts;
  const auto location = extractLocation(sourceManager, method.getLocation());
  const auto file = resolveFile(sourceManager, method.getLocation(), files);
  if (!location || !file)
    return facts;
  auto source = resolveRelationTarget(method, sourceManager, files, store);
  if (!source)
    return std::unexpected(source.error());
  if (!*source)
    return facts;
  for (const auto *base : method.overridden_methods()) {
    auto destination =
        resolveRelationTarget(*base, sourceManager, files, store);
    if (!destination)
      return std::unexpected(destination.error());
    if (!*destination)
      continue;
    const Relation relation{.source = **source,
                            .destination = **destination,
                            .kind = RelationKind::Overrides};
    facts.push_back({relation,
                     RelationSite{.source = **source,
                                  .destination = **destination,
                                  .kind = RelationKind::Overrides,
                                  .file = *file,
                                  .location = *location},
                     &method, base, method.getParent()});
  }
  return facts;
}

} // namespace facts
