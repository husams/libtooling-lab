#include "ast/extractors/OverrideRelation.h"

#include "ast/extractors/File.h"
#include "ast/extractors/Location.h"
#include "ast/extractors/NamedDecl.h"
#include "ast/extractors/TargetResolution.h"

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
  auto sourceUsr = extractUsr(method);
  if (!sourceUsr)
    return std::unexpected(sourceUsr.error());
  auto source =
      findOrStoreSymbolTarget(method, sourceManager, files, store, *sourceUsr);
  if (!source)
    return std::unexpected(ExtractionError::InvalidUsr);
  for (const auto *base : method.overridden_methods()) {
    auto usr = extractUsr(*base);
    if (!usr)
      return std::unexpected(usr.error());
    auto destination =
        findOrStoreSymbolTarget(*base, sourceManager, files, store, *usr);
    if (!destination)
      return std::unexpected(ExtractionError::InvalidUsr);
    const Relation relation{.source = *source,
                            .destination = *destination,
                            .kind = RelationKind::Overrides};
    facts.push_back({relation,
                     RelationSite{.source = *source,
                                  .destination = *destination,
                                  .kind = RelationKind::Overrides,
                                  .file = *file,
                                  .location = *location},
                     &method, base, method.getParent()});
  }
  return facts;
}

} // namespace facts
