#include "analysis/callgraph/CallGraphLinker.h"

#include "storage/FactStore.h"

#include <algorithm>
#include <ranges>
#include <tuple>

namespace facts::callgraph {
namespace {

auto relationKey(const Relation &value) {
  return std::tuple{value.source, value.destination, value.kind,
                    value.position};
}

auto siteKey(const RelationSite &value) {
  return std::tuple{
      value.source, value.destination,     value.kind,         value.position,
      value.file,   value.location.offset, value.receiverType, value.certainty};
}

template <typename Fact>
void append(std::vector<Relation> &relations, std::vector<RelationSite> &sites,
            const std::vector<Fact> &facts) {
  for (const auto &fact : facts) {
    relations.push_back(fact.relation);
    sites.push_back(fact.site);
  }
}

} // namespace

IndexingResult linkCallGraphFacts(CallGraphFacts facts, FactStore &store) {
  std::vector<Relation> relations;
  std::vector<RelationSite> sites;
  std::vector<ExternalReference> externalReferences;
  append(relations, sites, facts.calls);
  append(relations, sites, facts.overrides);
  append(relations, sites, facts.dispatches);
  for (const auto &fact : facts.calls) {
    if (!fact.externalTarget) {
      continue;
    }
    externalReferences.push_back(
        ExternalReference{.source = fact.site.source,
                          .destination = fact.site.destination,
                          .kind = fact.site.kind,
                          .position = fact.site.position,
                          .file = fact.site.file,
                          .offset = fact.site.location.offset,
                          .externalSymbol = fact.site.destination});
  }
  std::ranges::sort(relations, {}, relationKey);
  relations.erase(std::ranges::unique(relations, {}, relationKey).begin(),
                  relations.end());
  std::ranges::sort(sites, {}, siteKey);
  sites.erase(std::ranges::unique(sites, {}, siteKey).begin(), sites.end());
  if (relations.empty() && facts.entries.empty() && facts.unresolved.empty())
    return {};
  const auto entries =
      facts.entries | std::views::transform([](SymbolId id) {
        return CallGraphEntry{.symbolId = id, .graphNodeRef = id};
      }) |
      std::ranges::to<std::vector>();
  return store
      .addCallGraphFacts(relations, sites, externalReferences, entries,
                         facts.unresolved)
      .transform_error([](std::error_code error) {
        return IndexingError{"cannot persist call graph facts: " +
                             error.message()};
      });
}

} // namespace facts::callgraph
