#include "ast/extractors/CallableSite.h"

#include "ast/extractors/File.h"
#include "ast/extractors/Location.h"
#include "ast/extractors/NamedDecl.h"
#include "ast/extractors/Reference.h"
#include "ast/extractors/RelationTarget.h"
#include "storage/FactStore.h"

#include <clang/AST/DeclCXX.h>

namespace facts {
namespace {

// relation_site has no flags, so its existing position key partitions
// compiler-generated Calls from explicit Calls without changing the schema.
constexpr std::uint16_t invocationPosition(bool implicit) {
  return implicit ? 1 : 0;
}

} // namespace

ExtractionResult<std::optional<callgraph::CallFact>> extractCallableSite(
    const clang::FunctionDecl &caller, const clang::FunctionDecl &callee,
    clang::SourceLocation sourceLocation, ReceiverContext receiver,
    bool implicit, bool virtualDispatch,
    const clang::SourceManager &sourceManager, FileManager &files,
    FactStore &store) {
  const auto location = extractLocation(sourceManager, sourceLocation);
  if (!location)
    return std::nullopt;
  const auto file = resolveFile(sourceManager, sourceLocation, files);
  if (!file)
    return std::nullopt;
  const auto &sourceDecl = referenceOwner(caller);
  const auto &targetDecl = referenceOwner(callee);
  return extractUsr(sourceDecl)
      .and_then([&](std::string usr) {
        return store.findId(usr).transform_error(
            [](std::error_code) { return ExtractionError::InvalidUsr; });
      })
      .and_then([&](std::optional<SymbolId> source)
                    -> ExtractionResult<std::optional<callgraph::CallFact>> {
        if (!source)
          return std::nullopt;
        return resolveRelationTarget(targetDecl, sourceManager, files, store)
            .transform([&](std::optional<SymbolId> destination)
                           -> std::optional<callgraph::CallFact> {
              if (!destination)
                return std::nullopt;
              const auto position = invocationPosition(implicit);
              const Relation relation{.source = *source,
                                      .destination = *destination,
                                      .kind = RelationKind::Calls,
                                      .flags = static_cast<std::uint16_t>(
                                          implicit ? bit(ImplicitEdgeBit) : 0),
                                      .position = position};
              const auto *method =
                  llvm::dyn_cast<clang::CXXMethodDecl>(&targetDecl);
              return callgraph::CallFact{
                  relation,
                  RelationSite{.source = *source,
                               .destination = *destination,
                               .kind = RelationKind::Calls,
                               .position = position,
                               .file = *file,
                               .location = *location,
                               .receiverType = receiver.type,
                               .certainty = receiver.certainty},
                  &sourceDecl,
                  &targetDecl,
                  receiver.declaration,
                  virtualDispatch && method && method->isVirtual()};
            });
      });
}

} // namespace facts
