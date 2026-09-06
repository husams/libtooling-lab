from __future__ import annotations

from pytest_bdd import then

from support.database import require
from support.scenario import FactsToolContext
from support.s022 import graph, named_edges


@then("mixed cleanup and destructor subobjects retain site semantics")
def mixed_and_subobjects(context: FactsToolContext) -> None:
    mixed = [(edge, target) for edge, target in
             named_edges(graph(context, "s022_fixture::mixedCleanup"))
             if edge["depth"] == 1 and edge["relation_kind"] == "Calls" and
             target == "s022_fixture::Derived::~Derived"]
    require(len(mixed) == 2 and {edge["implicit"] for edge, _ in mixed} ==
            {False, True}, str(mixed))
    contexts = {(edge["implicit"], edge["certainty"],
                 bool(edge["receiver_type_id"])) for edge, _ in mixed}
    require(contexts == {(False, "possible", False), (True, "exact", True)},
            str(mixed))
    subobjects = [(edge, target) for edge, target in
                  named_edges(graph(context, "s022_fixture::Derived::~Derived"))
                  if edge["depth"] == 1 and edge["relation_kind"] == "Calls" and
                  edge["semantic_kind"] == "destructor"]
    require({target for _, target in subobjects} ==
            {"s022_fixture::Base::~Base", "s022_fixture::Member::~Member"},
            str(subobjects))
    require(all(edge["implicit"] and edge["certainty"] == "exact" and
                edge["receiver_type_id"] for edge, _ in subobjects),
            str(subobjects))
    require(all(edge["location"]["path"].endswith("service.cpp")
                for edge, _ in subobjects), str(subobjects))
    dispatches = [(edge, target) for edge, target in
                  named_edges(graph(context, "s022_fixture::Derived::~Derived"))
                  if edge["depth"] == 1 and
                  edge["relation_kind"] == "DispatchCalls"]
    require(all(target != "s022_fixture::Derived::~Derived"
                for _, target in dispatches), str(dispatches))


@then("lifecycle receiver evidence and destructor dispatch are preserved")
def receiver_and_dispatch(context: FactsToolContext) -> None:
    run_edges = named_edges(graph(context, "s022_fixture::run"))
    lifetimes = [(edge, target) for edge, target in run_edges
                 if edge["depth"] == 1 and edge["semantic_kind"] in
                 {"constructor", "destructor"}]
    require(lifetimes and all(edge["certainty"] == "exact" and
                              edge["receiver_type_id"] for edge, _ in lifetimes),
            str(lifetimes))
    deleted = [(edge["relation_kind"], target, edge["certainty"],
                edge["receiver_type_id"]) for edge, target in
               named_edges(graph(context, "s022_fixture::deleteBase"))
               if edge["depth"] == 1]
    require(set(deleted) ==
            {("Calls", "s022_fixture::Base::~Base", "possible", None),
             ("DispatchCalls", "s022_fixture::Derived::~Derived", "possible", None)},
            str(deleted))


@then("implicit definitions and the raw Calls contract remain accurate")
def implicit_and_raw(context: FactsToolContext) -> None:
    semantic = graph(context, "s022_fixture::implicitConstruction")
    targets = {edge["target_id"] for edge in semantic["edges"]
               if edge["depth"] == 1 and edge["semantic_kind"] == "constructor"}
    constructors = [node for node in semantic["nodes"] if node["id"] in targets]
    require(len(constructors) == 2 and all(node["facts"]["implicit"] and
            node["definition_availability"] == "available"
            for node in constructors), str(constructors))
    require(semantic["extraction_coverage"]["state"] != "incomplete",
            str(semantic["extraction_coverage"]))
    raw = graph(context, "s022_fixture::run", "calls")
    require(raw["edge_view"] == "calls" and
            len(raw["edges"]) == len(graph(context, "s022_fixture::run")["edges"]) and
            all("semantic_kind" not in edge for edge in raw["edges"]), str(raw))
