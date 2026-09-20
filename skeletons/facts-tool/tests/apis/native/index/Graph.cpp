#include "Fixture.h"
#include "apis/v2/JobServices.h"
#include "storage/Storage.h"
#include <array>
#include <map>
#include <set>

namespace index_test {
namespace {
facts::SymbolId function(facts::Storage &store, const std::string &name,
                         facts::FileId definition) {
  facts::Function value{};
  value.Kind = clang::index::SymbolKind::Function;
  value.usr = "usr:" + name;
  value.qualifiedName = name;
  value.flags |= facts::bit(facts::DefinitionBit);
  value.definition = facts::Region{100, 20};
  value.definitionFile = definition;
  value.loc = {10, 3, 40};
  const auto id = store.save(3, value);
  assert(id);
  return *id;
}
void edge(facts::Storage &store, facts::SymbolId source,
          facts::SymbolId target, facts::FileId file) {
  const std::array relations{facts::Relation{source, target, facts::RelationKind::Calls}};
  const std::array sites{facts::RelationSite{source, target, facts::RelationKind::Calls,
      0, file, {12, 5, 110}}};
  const std::array entries{facts::CallGraphEntry{source, source}, facts::CallGraphEntry{target, target}};
  assert(store.addCallGraphFacts(relations, sites, {}, entries));
}
}
void verifyGraphIdentity() {
  Fixture fixture;
  // The shared header is in a component outside the repository, while the
  // definitions live in its two registered source files.
  execute(fixture.project, R"sql(
UPDATE component SET repository_id=1,path='b' WHERE id=3;
INSERT INTO component(id,name,path) VALUES(4,'headers','/headers');
INSERT INTO directory(id,component_id,path) VALUES(3,4,'.');
UPDATE file SET directory_id=3 WHERE id=3;
)sql");
  fs::remove(fixture.first);
  fs::remove(fixture.second);
  facts::SymbolId a, firstB, secondB, c;
  {
    facts::Storage first(fixture.first.string());
    a = function(first, "A", 1);
    firstB = function(first, "B", 2);
    edge(first, a, firstB, 1);
  }
  {
    facts::Storage second(fixture.second.string());
    secondB = function(second, "B", 2);
    c = function(second, "C", 2);
    edge(second, secondB, c, 2);
  }
  // Per-database symbol allocators produce colliding numeric IDs for unrelated
  // functions in a common header. Public graph identities must use the USR.
  assert(a == secondB && firstB == c);
  assert(index::refresh(fixture.project));
  facts::apis::domain::Context context;
  context.configuration.database = fixture.project;
  context.configuration.projectRoot = fixture.root;
  facts::apis::runtime::Request request;
  request.operation = "callgraphs";
  request.options = {{"root", {{"qualified_name", "A"}, {"repository", "alpha"}}}};
  const auto graph = facts::apis::v2::jobs::callGraph(context, request);
  assert(graph);
  assert(graph->at("node_count") == 3 && graph->at("edge_count") == 2);
  std::map<std::string, std::string> ids;
  for (const auto &node : graph->at("nodes"))
    ids.emplace(node.at("qualified_name").get<std::string>(), node.at("symbol_id").get<std::string>());
  assert(ids.at("A") == index::symbolIdentity("usr:A", 1, true));
  assert(ids.at("B") == index::symbolIdentity("usr:B", 1, true));
  assert(ids.at("C") == index::symbolIdentity("usr:C", 1, true));
  std::set<std::pair<std::string, std::string>> edges;
  for (const auto &edge : graph->at("edges"))
    edges.emplace(edge.at("source").get<std::string>(), edge.at("target").get<std::string>());
  assert((edges == std::set<std::pair<std::string, std::string>>{
      {ids.at("A"), ids.at("B")}, {ids.at("B"), ids.at("C")}}));
}
}
