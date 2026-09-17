#include "analysis/callgraph/CallGraphEntryQuery.h"
#include "analysis/callgraph/CallGraphQuery.h"
#include "analysis/callgraph/CallGraphTraversal.h"
#include "storage/Storage.h"

#include <llvm/Support/JSON.h>

#include <array>
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace {
using namespace facts;
using namespace facts::callgraph;

void require(bool condition, std::string_view message) {
  if (!condition)
    throw std::runtime_error(std::string{message});
}

SymbolId seed(const std::string &path) {
  Storage store(path);
  Function caller{};
  caller.usr = "usr:caller";
  caller.qualifiedName = "caller";
  caller.Kind = clang::index::SymbolKind::Function;
  caller.flags |= bit(DefinitionBit);
  caller.definition = Region{0, 100};
  Variable pointer{};
  pointer.usr = "usr:caller:fp";
  pointer.qualifiedName = "caller::fp";
  pointer.Kind = clang::index::SymbolKind::Variable;
  const auto source = store.save(1, caller);
  const auto target = store.save(1, pointer);
  require(source && target, "cannot seed caller and pointer variable");
  const std::array calls{
      PointerCallSite{*source, *target, 1, {3, 5, 30}, "void (*)(int)", "fp"},
      PointerCallSite{*source, std::nullopt, 1, {4, 5, 40}, "void (*)(int)",
                      "choose()"}};
  const std::array entries{CallGraphEntry{*source, *source}};
  require(store.addCallGraphFacts({}, {}, {}, entries, {}, calls).has_value(),
          "cannot seed pointer call evidence");
  return *source;
}

void verify(const std::string &path, SymbolId source) {
  const auto graph = loadCallGraph(path);
  require(graph.has_value(), graph ? "" : graph.error());
  require(graph->nodes.size() == 1 && graph->nodes.front().id == source,
          "pointer operand leaked into function graph nodes");
  const auto &node = graph->nodes.front();
  const auto roots = selectRoots(*graph, std::nullopt, true);
  require(roots && roots->size() == 1 && roots->front()->id == source,
          "all-roots selection omitted a pointer-call-only function");
  require(node.pointerCalls == 2 && node.unresolved == 0,
          "pointer calls were counted as unresolved targets");
  require(graph->edges.empty() && graph->pointerCalls.size() == 2,
          "pointer calls must remain separate from function traversal edges");
  require(graph->pointerCalls.front().target &&
              graph->pointerCalls.front().target->usr == "usr:caller:fp" &&
              !graph->pointerCalls.back().target,
          "pointer operand identity or absent identity was lost");
  const auto traversal = traverseCallGraph(*graph, {&node}, std::nullopt);
  require(traversal.nodes == std::vector{source} && traversal.edges.empty(),
          "traversal expanded the pointer operand as a function");
  auto entry = loadCallGraphEntry(path, source);
  require(entry.has_value(), entry ? "" : entry.error());
  require(entry->entry && entry->pointerCalls.size() == 2 &&
              entry->externalTargets.empty(),
          "entry lost pointer evidence or classified an operand external");
  // Even a caller supplying the old edge-only leaf calculation must not
  // present a pointer-call-only function as a leaf.
  entry->leaf = true;
  auto json = llvm::json::parse(renderCallGraphEntryJson(node, *entry));
  require(json && json->getAsObject(), "entry is not valid JSON");
  const auto *object = json->getAsObject();
  require(object->getBoolean("is_leaf") == false,
          "pointer-call-only entry was rendered as a leaf");
  const auto *calls = object->getArray("pointer_calls");
  require(calls && calls->size() == 2, "JSON omitted pointer call sites");
  const auto *first = calls->front().getAsObject();
  require(first && first->getString("kind") == "pointer-call" &&
              first->getString("signature") == "void (*)(int)" &&
              first->getString("expression") == "fp" &&
              first->getObject("target") &&
              first->getObject("target")->getString("usr") ==
                  "usr:caller:fp" &&
              first->getObject("site") &&
              first->getObject("site")->getInteger("offset") == 30,
          "JSON omitted typed operand identity or source location");
  const auto *coverage = object->getObject("coverage");
  require(coverage && coverage->getInteger("pointer_calls") == 2 &&
              coverage->getInteger("unresolved_targets") == 0,
          "JSON combined pointer calls and unresolved targets");
  const auto text = renderCallGraphEntryText(node, *entry);
  require(text.contains("is_leaf=false") &&
              text.contains("pointer_calls=2") &&
              text.contains("signature=void (*)(int) expression=fp") &&
              text.contains("pointer-call target=null"),
          "text omitted pointer-call evidence");
}

void verifyLegacy(const std::string &path, SymbolId source) {
  {
    auto database = storage::Database::open(path, storage::Database::readWrite);
    require(database &&
                database->execute("DROP TABLE callgraph_pointer_call_site"),
            "cannot simulate a legacy facts database");
  }
  const auto graph = loadCallGraph(path);
  require(graph && graph->nodes.size() == 1 && graph->pointerCalls.empty() &&
              graph->nodes.front().pointerCalls == 0,
          "legacy facts database did not read with empty pointer evidence");
  const auto entry = loadCallGraphEntry(path, source);
  require(entry && entry->entry && entry->pointerCalls.empty(),
          "legacy call graph entry did not remain readable");
}
} // namespace

int main(int argc, char **argv) {
  if (argc != 2)
    return 1;
  const std::string path = argv[1];
  std::filesystem::remove(path);
  try {
    const auto source = seed(path);
    verify(path, source);
    verifyLegacy(path, source);
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    std::filesystem::remove(path);
    return 1;
  }
  std::filesystem::remove(path);
  return 0;
}
