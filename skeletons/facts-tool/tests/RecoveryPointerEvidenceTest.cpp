#include "commands/analyse/CallGraphRecoveryInternal.h"
#include "commands/analyse/RecoveryEvidence.h"
#include "commands/analyse/RecoveryScan.h"

#include <iostream>
#include <stdexcept>

namespace {
using namespace facts;
using namespace facts::callgraph;
using namespace facts::commands;

void require(bool condition, std::string_view message) {
  if (!condition)
    throw std::runtime_error(std::string{message});
}

struct Fixture {
  RecoveryContext context;
  RecoveryCandidate candidate;
  QueryGraph graph;

  Fixture() {
    catalog::File file;
    file.id = 1;
    file.component.path = "/tmp/pointer-recovery";
    file.name = "caller.cpp";
    context.files.emplace(1, file);
    QueryNode caller{{1, 1}, "caller", "usr:caller", true};
    caller.definitionLocation = QueryDefinition{1, 0, 100};
    caller.bodyEvidence = true;
    caller.pointerCalls = 2;
    graph.nodes.push_back(caller);
    graph.pointerCalls = {
        {{caller.id, SymbolId{1, 2}, 1, {3, 5, 30}, "void (*)(int)", "fp"},
         PointerCallTarget{{1, 2}, "caller::fp", "usr:caller:fp"}},
        {{caller.id, std::nullopt, 1, {4, 5, 40}, "void (*)(int)", "choose()"},
         std::nullopt}};
    candidate.entry.tuFileId = 1;
    candidate.entry.relatedUsrs = {caller.usr};
    auto scan = std::make_shared<RecoveryScan>();
    scan->status = 0;
    scan->facts.calls[caller.usr] = {};
    scan->facts.unresolved[caller.usr] = 0;
    scan->facts.definitions[caller.usr] = {
        "/tmp/pointer-recovery/caller.cpp", 0, 100};
    scan->facts.pointerCalls[caller.usr] = {
        {"usr:caller:fp", "/tmp/pointer-recovery/caller.cpp", 30, 3, 5,
         "void (*)(int)", "fp"},
        {std::nullopt, "/tmp/pointer-recovery/caller.cpp", 40, 4, 5,
         "void (*)(int)", "choose()"}};
    context.scans.emplace(1, std::move(scan));
  }

  auto validate(const QueryGraph &value) {
    return validateRecoveryEvidence(context, {}, value, candidate);
  }

  template <typename Change>
  void rejects(Change change, std::string_view message) {
    auto mutated = graph;
    change(mutated);
    const auto result = validate(mutated);
    require(!result && result.error() == "persisted pointer-call evidence changed",
            message);
  }
};

void check() {
  Fixture fixture;
  require(fixture.validate(fixture.graph).has_value(),
          "unchanged pointer evidence was rejected");
  fixture.rejects([](auto &g) { g.pointerCalls.front().target->usr = "other"; },
                  "changed operand USR was reused");
  fixture.rejects([](auto &g) { g.pointerCalls.front().site.signature = "int (*)()"; },
                  "changed callable signature was reused");
  fixture.rejects([](auto &g) { g.pointerCalls.front().site.expression = "other"; },
                  "changed pointer expression was reused");
  fixture.rejects([](auto &g) { ++g.pointerCalls.front().site.location.offset; },
                  "changed pointer offset was reused");
  fixture.rejects([](auto &g) { ++g.pointerCalls.front().site.location.line; },
                  "changed pointer line was reused");
  fixture.rejects([](auto &g) { ++g.pointerCalls.front().site.location.column; },
                  "changed pointer column was reused");
  fixture.rejects([](auto &g) { g.pointerCalls.pop_back(); },
                  "missing expression-only invocation was reused");
  fixture.rejects([](auto &g) {
    g.pointerCalls.front().site.target.reset();
    g.pointerCalls.front().target.reset();
  }, "lost operand identity was reused");
  auto renumbered = fixture.graph;
  renumbered.pointerCalls.front().site.target = SymbolId{7, 9};
  renumbered.pointerCalls.front().target->id = SymbolId{7, 9};
  require(fixture.validate(renumbered).has_value(),
          "stable operand USR was rejected after native ID renumbering");
}
} // namespace

int main() {
  try {
    check();
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
  return 0;
}
