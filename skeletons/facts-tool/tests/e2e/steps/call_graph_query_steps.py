from __future__ import annotations

from pytest_bdd import given, then
from support import callgraph_run as cg
from support import s024_paths as s024
from support.database import require
from support.s024_fixture import prepare
from support.scenario import FactsToolContext


@given("the S-024 multi-component call graph corpus is extracted")
def corpus(context: FactsToolContext) -> None:
    prepare(context)


@then("S-024 forward traversal stays default and callers are explicit")
def callers(context: FactsToolContext) -> None:
    facts = context.facts_database_path
    forward = cg.run_graph(context, "--function", "s024_fixture::source")
    reverse = cg.run_graph(context, "--function", "s024_fixture::source",
                           "--direction", "callers")
    forward_id, _ = cg.completion(forward)
    reverse_id, _ = cg.completion(reverse)
    require(forward.returncode == reverse.returncode == 0, forward.stderr + reverse.stderr)
    require(cg.run_row(facts, forward_id)["mode"] == "callees", forward_id)
    require(cg.run_row(facts, reverse_id)["mode"] == "callers", reverse_id)
    forward_sources = {source for source, _ in cg.edge_names(facts, forward_id)}
    reverse_sources = {source for source, _ in cg.edge_names(facts, reverse_id)}
    require("s024_fixture::onlyCaller" not in forward_sources, str(forward_sources))
    require("s024_fixture::onlyCaller" in reverse_sources, str(reverse_sources))


@then("S-024 paths cross components with deterministic cycle semantics")
def paths(context: FactsToolContext) -> None:
    facts = context.facts_database_path
    args = ("--function", s024.SOURCE, "--to", s024.TARGET)
    first_id, _ = cg.completion(cg.run_graph(context, *args))
    second_id, _ = cg.completion(cg.run_graph(context, *args))
    all_first_id, _ = cg.completion(cg.run_graph(context, *args, "--path-mode", "all-simple"))
    all_second_id, _ = cg.completion(cg.run_graph(context, *args, "--path-mode", "all-simple"))
    require(cg.edges(facts, first_id) == cg.edges(facts, second_id),
            "shortest path edges changed between runs")
    require(cg.edges(facts, all_first_id) == cg.edges(facts, all_second_id),
            "all-simple path edges changed between runs")
    shortest_edges = cg.edges(facts, first_id)
    require(s024.edge_triples(shortest_edges) == s024.SHORTEST_EDGES and
            [edge["depth"] for edge in shortest_edges] == [1, 2], str(shortest_edges))
    all_edges = cg.edges(facts, all_first_id)
    require(s024.edge_triples(all_edges) == s024.ALL_SIMPLE_EDGES, str(all_edges))
    require(len(all_edges) == len(s024.ALL_SIMPLE_EDGES), "duplicate persisted sites")
    require(all(edge["cycle"] == 0 for edge in all_edges), "a cycle edge was persisted")
    found_paths = s024.simple_paths(all_edges, s024.SOURCE, s024.TARGET)
    require(found_paths == s024.ALL_SIMPLE_PATHS, str(found_paths))
    require({(e["source"], e["target"]) for e in all_edges} ==
            s024.edges_on_paths(found_paths), "an edge lies on no node-simple path")
    dispatch_edges = [edge for edge in all_edges if edge["kind"] == s024.DISPATCH]
    for edge in dispatch_edges:
        certainties = cg.query(facts, "SELECT certainty FROM relation_site WHERE "
            "source_id=? AND destination_id=? AND kind=? AND position=? AND "
            "file_id=? AND offset=?", (edge["source_id"], edge["target_id"], edge["kind"],
            edge["position"], edge["file_id"], edge["offset"]))
        require(certainties == [(2,)], str(certainties))
    self_result = cg.run_graph(context, "--function", s024.SOURCE, "--to", s024.SOURCE,
                               "--path-mode", "all-simple")
    self_id, self_status = cg.completion(self_result)
    require(self_status == "complete", self_status)
    self_target = cg.target(facts, self_id)
    require(self_target is not None and self_target[0] == s024.SOURCE, str(self_target))
    require(not cg.edges(facts, self_id), "self path must record no edges")


@then("S-024 selector and option failures are explicit usage errors")
def failures(context: FactsToolContext) -> None:
    facts = context.facts_database_path
    before = cg.run_count(facts)
    bad = [("--function", "s024_fixture::overloaded", "--direction", "callers"),
           ("--all", "--to", "s024_fixture::target"),
           ("--function", "s024_fixture::source", "--direction", "callers",
            "--to", "s024_fixture::target"),
           ("--function", "s024_fixture::source", "--path-mode", "all-simple"),
           ("--function", "s024_fixture::source", "--to", "missing")]
    results = [cg.run_graph(context, *args) for args in bad]
    require(all(result.returncode == 2 for result in results),
            str([result.stderr for result in results]))
    require(all(len(cg.stderr_lines(result)) == 1 for result in results),
            str([result.stderr for result in results]))
    require("ambiguous-root" in results[0].stderr and results[0].stderr.count("usr='") >= 2,
            results[0].stderr)
    require("--to is incompatible with --all" in results[1].stderr and
            "--to is incompatible with --direction callers" in results[2].stderr and
            "--path-mode requires --to" in results[3].stderr and
            "missing-target" in results[4].stderr, str([r.stderr for r in results]))
    require(cg.run_count(facts) == before, "usage errors persisted a run")


@then("path runs distinguish found, unreachable and truncated evidence")
def path_evidence(context: FactsToolContext) -> None:
    facts = context.facts_database_path
    found_id, found_status = cg.completion(cg.run_graph(
        context, "--function", "s024_fixture::source", "--to", "s024_fixture::target"))
    require(found_status == "complete", found_id)
    found_edges = cg.edges(facts, found_id)
    require(any(edge["target"] == "s024_fixture::target" for edge in found_edges),
            str(found_edges))
    require(len(found_edges) == 2, "expected only the shortest-path edges to persist: "
            + str(found_edges))

    def assert_isolated(run_id: int, status: str) -> None:
        require(status == "complete", run_id)
        target_row = cg.target(facts, run_id)
        require(target_row is not None and target_row[0] == "s024_fixture::isolated",
                str(target_row))
        edges = cg.edges(facts, run_id)
        require(not any(edge["target"] == "s024_fixture::isolated" for edge in edges),
                str(edges))

    isolated_id, isolated_status = cg.completion(cg.run_graph(
        context, "--function", "s024_fixture::source", "--to", "s024_fixture::isolated"))
    assert_isolated(isolated_id, isolated_status)

    isolated_unconf_id, isolated_unconf_status = cg.completion(cg.run_graph(
        context, "--function", "s024_fixture::source", "--to", "s024_fixture::isolated",
        conf=False))
    assert_isolated(isolated_unconf_id, isolated_unconf_status)

    truncated_id, truncated_status = cg.completion(cg.run_graph(
        context, "--function", "s024_fixture::source", "--to", "s024_fixture::target",
        "--max-depth", "1"))
    require(truncated_status == "truncated" and
            cg.run_row(facts, truncated_id)["truncation_reason"] == "max_depth",
            truncated_id)

    no_callers_id, no_callers_status = cg.completion(cg.run_graph(
        context, "--function", "s024_fixture::isolated", "--direction", "callers"))
    require(no_callers_status == "complete" and not cg.edges(facts, no_callers_id),
            no_callers_id)
