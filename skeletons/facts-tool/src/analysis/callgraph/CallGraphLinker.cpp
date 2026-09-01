#include "analysis/callgraph/CallGraphLinker.h"

#include "storage/FactStore.h"

#include <algorithm>
#include <ranges>
#include <tuple>

namespace facts::callgraph {
namespace {

auto relationKey(const Relation &value) {
  return std::tuple{value.source, value.destination, value.kind, value.position};
}

auto siteKey(const RelationSite &value) {
  return std::tuple{value.source, value.destination, value.kind, value.position,
                    value.file, value.location.offset, value.receiverType,
                    value.certainty};
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
  append(relations, sites, facts.calls);
  append(relations, sites, facts.overrides);
  append(relations, sites, facts.dispatches);
  std::ranges::sort(relations, {}, relationKey);
  relations.erase(std::ranges::unique(relations, {}, relationKey).begin(),
                  relations.end());
  std::ranges::sort(sites, {}, siteKey);
  sites.erase(std::ranges::unique(sites, {}, siteKey).begin(), sites.end());
  if (relations.empty())
    return {};
  return store.addRelationFacts(relations, sites)
      .transform_error([](std::error_code error) {
        return IndexingError{"cannot persist call graph facts: " +
                             error.message()};
      });
}

} // namespace facts::callgraph
