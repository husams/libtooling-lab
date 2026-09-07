from __future__ import annotations

from pytest_bdd import then
from steps.call_graph_scope_steps import graph, node_set, run
from support import callgraph_run as cg
from support.database import require
from support.scenario import FactsToolContext


def table_count(facts, table: str) -> int:
    return cg.query(facts, f"SELECT COUNT(*) FROM {table}")[0][0]


@then("each explicit structural budget reports its exact frontier")
def structural_budgets(context: FactsToolContext) -> None:
    facts = context.facts_database_path
    cases = (("--max-depth", "1", "max_depth"), ("--max-nodes", "2", "max_nodes"),
             ("--max-edges", "1", "max_edges"))
    for flag, limit, reason in cases:
        result = graph(context, flag, limit)
        run_id, status = cg.completion(result)
        require(result.returncode == 0 and status == "truncated",
                result.stdout + result.stderr)
        row = cg.run_row(facts, run_id)
        require(row["truncation_reason"] == reason, str(row))
        require(cg.frontier(facts, run_id), "expected a non-empty frontier")
        require(len(cg.edges(facts, run_id)) == 1 and
                len(node_set(context, run_id)) == 2, str(cg.edges(facts, run_id)))


@then("a bounded all-root traversal preserves every skipped root")
def all_root_frontier(context: FactsToolContext) -> None:
    facts = context.facts_database_path
    result = cg.run_graph(context, "--all", "--max-nodes", "1")
    run_id, status = cg.completion(result)
    require(result.returncode == 0 and status == "truncated", result.stdout + result.stderr)
    require(cg.run_row(facts, run_id)["truncation_reason"] == "max_nodes", run_id)
    frontier_names = {name for name, _ in cg.frontier(facts, run_id)}
    require(frontier_names == {"scope_fixture::b", "scope_fixture::c",
                               "scope_fixture::leaf"}, str(frontier_names))


@then("an exact-depth leaf is complete and cycles terminate without a cap")
def leaf_and_cycle(context: FactsToolContext) -> None:
    facts = context.facts_database_path
    leaf = cg.run_graph(context, "--function", "scope_fixture::leaf", "--max-depth", "1")
    leaf_id, leaf_status = cg.completion(leaf)
    require(leaf.returncode == 0 and leaf_status == "complete", leaf.stdout + leaf.stderr)
    require(cg.run_row(facts, leaf_id)["truncation_reason"] is None, leaf_id)
    require(not cg.frontier(facts, leaf_id), "leaf traversal must not truncate")
    cycle = graph(context)
    cycle_id, cycle_status = cg.completion(cycle)
    require(cycle_status == "complete", cycle_status)
    require(any(edge["cycle"] for edge in cg.edges(facts, cycle_id)), "expected a cycle edge")


@then("invalid graph budgets fail as usage errors and requests persist only their run")
def invalid_and_request_local(context: FactsToolContext) -> None:
    facts = context.facts_database_path
    before_runs = cg.run_count(facts)
    for flag in ("--max-depth", "--max-nodes", "--max-edges", "--time-limit-ms"):
        for value in ("0", "-1", "abc"):
            output = graph(context, flag, value)
            require(output.returncode == 2 and flag in output.stderr,
                    output.stdout + output.stderr)
    require(cg.run_count(facts) == before_runs, "invalid budgets persisted a run")
    conf_before = context.files_database_path.read_bytes()
    symbol_before = table_count(facts, "symbol")
    relation_before = table_count(facts, "relation")
    site_before = table_count(facts, "relation_site")
    result = graph(context, "--component", "A", "--max-nodes", "1")
    require(result.returncode == 0, result.stdout + result.stderr)
    require(cg.run_count(facts) == before_runs + 1,
            "the successful filtered run must persist exactly one run row")
    require(table_count(facts, "symbol") == symbol_before and
            table_count(facts, "relation") == relation_before and
            table_count(facts, "relation_site") == site_before,
            "request-local filters mutated extracted facts")
    require(context.files_database_path.read_bytes() == conf_before,
            "request-local filters mutated the project store")


@then("operational failures before traversal exit one with a single stderr line and no run")
def operational_error(context: FactsToolContext) -> None:
    facts = context.facts_database_path
    before = cg.run_count(facts)
    missing = context.run_root_path / "missing.sqlite"
    output = run([str(context.facts_tool), "analyse", "call-graph", "-v", "0", "-f",
                 str(missing), "--all"])
    require(output.returncode == 1, output.stdout + output.stderr)
    require(output.stdout == "" and len(cg.stderr_lines(output)) == 1,
            output.stdout + output.stderr)
    require(cg.run_count(facts) == before, "operational failure left a run")
