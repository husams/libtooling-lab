from __future__ import annotations

from pytest_bdd import given, then

from support.database import file_snapshot, require
from support.scenario import FactsToolContext
from support.s022 import (definition_available, definition_path, graph,
                           named_edges, run, source_path)


@given("the S-022 multi-component corpus is extracted")
def extract_s022(context: FactsToolContext) -> None:
    context.prepare()
    root = context.fixture_root / "s022"
    sources = [root / "alpha" / "entry.cpp", root / "beta" / "service.cpp"]
    components = [f"s022-alpha={root / 'alpha'}", f"s022-beta={root / 'beta'}"]
    imported = run([str(context.facts_tool), "import", "-v", "0", "-c",
                    str(context.files_database_path),
                    *[f"--component={value}" for value in components],
                    "--extra-arg=-std=c++23", *map(str, sources)])
    require(imported.returncode == 0, imported.stdout + imported.stderr)
    extracted = run([str(context.facts_tool), "extract", "-v", "0", "-c",
                     str(context.files_database_path), "-o",
                     str(context.facts_database_path), *map(str, sources)])
    context.last_output = extracted.stdout + extracted.stderr
    require(extracted.returncode == 0, context.last_output)


@then("explicit and implicit construction and cleanup edges are semantic")
def callable_lifetimes(context: FactsToolContext) -> None:
    expected = {"run": True, "heapCleanup": True, "temporaryCleanup": True,
                "explicitCleanup": False}
    for root, implicit in expected.items():
        edges = named_edges(graph(context, f"s022_fixture::{root}"))
        cleanup = [(edge, target) for edge, target in edges
                   if edge["semantic_kind"] == "destructor" and edge["depth"] == 1]
        sites = {(edge["target"], edge["file_id"], edge["offset"]) for edge, _ in cleanup}
        require(len(sites) == 1 and all(edge["implicit"] is implicit for edge, _ in cleanup),
                f"bad cleanup for {root}: {cleanup}")
        require(cleanup[0][0]["location"]["path"].endswith("entry.cpp"), str(cleanup))
    constructors = named_edges(graph(context, "s022_fixture::run"))
    require(any(edge["semantic_kind"] == "constructor" and not edge["implicit"]
                and target == "s022_fixture::Derived::Derived"
                for edge, target in constructors), str(constructors))


@then("the lambda edge is unique and cross-component targets resolve")
def lambda_and_components(context: FactsToolContext) -> None:
    document = graph(context, "s022_fixture::run")
    edges = named_edges(document)
    lambdas = [(edge, target) for edge, target in edges
               if edge["semantic_kind"] == "lambda" and edge["depth"] == 1]
    require(len(lambdas) == 1 and "::<lambda@" in lambdas[0][1], str(lambdas))
    constructor_id = next(edge["target_id"] for edge, target in edges
                          if target == "s022_fixture::Derived::Derived")
    facts = context.facts_database_path
    paths = dict(file_snapshot(context.files_database_path))
    require(definition_available(facts, constructor_id), str(constructor_id))
    require(source_path(paths, constructor_id).endswith("service.hpp") and
            definition_path(facts, paths, constructor_id).endswith("service.cpp"),
            str(constructor_id))


@then("static targets and dispatch expansions retain receiver certainty")
def dispatch_certainty(context: FactsToolContext) -> None:
    exact = [(edge, target) for edge, target in
             named_edges(graph(context, "s022_fixture::exactDispatch"))
             if edge["depth"] == 1 and edge["certainty"] == "exact" and
             target == "s022_fixture::Derived::value"]
    require({(edge["relation_kind"], target) for edge, target in exact} ==
            {("Calls", "s022_fixture::Derived::value"),
             ("DispatchCalls", "s022_fixture::Derived::value")}, str(exact))
    require(all(edge["receiver_type_id"] and
                edge["receiver"] == "s022_fixture::Derived" for edge, _ in exact), str(exact))
    possible = [(edge, target) for edge, target in
                named_edges(graph(context, "s022_fixture::invoke")) if edge["depth"] == 1]
    require({(edge["relation_kind"], target, edge["certainty"]) for edge, target in possible} ==
            {("Calls", "s022_fixture::Base::value", "possible"),
             ("DispatchCalls", "s022_fixture::Derived::value", "possible")}, str(possible))
    require(all(edge["receiver_type_id"] is None for edge, _ in possible), str(possible))


@then("exposes raw relation kinds and unsupported frontend semantics")
def compatibility_and_coverage(context: FactsToolContext) -> None:
    document = graph(context, "s022_fixture::run")
    require(all(edge["kind"] in {1, 18} for edge in document["edges"]), str(document["edges"]))
    require("coverage.unsupported_semantics kind=indirect-call" in context.last_output and
            "entry.cpp:39:42" in context.last_output and
            "service.hpp:30:" in context.last_output, context.last_output)
