from __future__ import annotations

import json
from pytest_bdd import then
from steps.call_graph_scope_steps import graph, run
from support.database import require
from support.scenario import FactsToolContext


@then("each explicit structural budget reports its exact frontier")
def structural_budgets(context: FactsToolContext) -> None:
    cases = (("--max-depth", "1", "max_depth", 2, 1),
             ("--max-nodes", "2", "max_nodes", 2, 1),
             ("--max-edges", "1", "max_edges", 2, 1))
    for flag, limit, reason, nodes, edges in cases:
        output = graph(context, flag, limit)
        require(output.returncode == 0, output.stdout + output.stderr)
        value = json.loads(output.stdout)
        require(value["truncation"]["reached"] and
                value["truncation"]["reason"] == reason and
                value["truncation"]["frontier"] and
                len(value["nodes"]) == nodes and len(value["edges"]) == edges and
                not value["coverage"]["traversal_complete"], str(value))


@then("a bounded all-root traversal preserves every skipped root")
def all_root_frontier(context: FactsToolContext) -> None:
    output = run([str(context.facts_tool), "analyse", "call-graph", "-v", "0",
                  "-f", str(context.facts_database_path), "-c",
                  str(context.files_database_path), "--format", "json", "--all",
                  "--max-nodes", "1"])
    require(output.returncode == 0, output.stdout + output.stderr)
    value = json.loads(output.stdout)
    frontier = {item["name"] for item in value["truncation"]["frontier"]}
    require(frontier == {"scope_fixture::b", "scope_fixture::c",
                         "scope_fixture::leaf"} and
            value["truncation"]["reason"] == "max_nodes", str(value))


@then("an exact-depth leaf is complete and cycles terminate without a cap")
def leaf_and_cycle(context: FactsToolContext) -> None:
    leaf = run([str(context.facts_tool), "analyse", "call-graph", "-v", "0",
                "-f", str(context.facts_database_path), "--format", "json",
                "--function", "scope_fixture::leaf", "--max-depth", "1"])
    require(leaf.returncode == 0, leaf.stdout + leaf.stderr)
    leaf_value = json.loads(leaf.stdout)
    cycle_value = json.loads(graph(context).stdout)
    require(leaf_value["truncation"]["reason"] is None and leaf_value["complete"],
            str(leaf_value))
    require(cycle_value["complete"] and any(edge["cycle"] for edge in
                                             cycle_value["edges"]), str(cycle_value))


@then("invalid graph budgets fail as usage errors and requests leave no cache")
def invalid_and_request_local(context: FactsToolContext) -> None:
    before = (context.files_database_path.read_bytes(),
              context.facts_database_path.read_bytes())
    for flag in ("--max-depth", "--max-nodes", "--max-edges", "--time-limit-ms"):
        for value in ("0", "-1", "abc"):
            output = graph(context, flag, value)
            require(output.returncode == 2 and flag in output.stderr,
                    output.stdout + output.stderr)
    graph(context, "--component", "A", "--max-nodes", "1")
    after = (context.files_database_path.read_bytes(),
             context.facts_database_path.read_bytes())
    require(before == after, "request-local filters or budgets changed stored state")


@then("operational JSON failures report error truncation and exit one")
def operational_error(context: FactsToolContext) -> None:
    output = run([str(context.facts_tool), "analyse", "call-graph", "-v", "0",
                  "-f", str(context.run_root_path / "missing.sqlite"),
                  "--format", "json", "--all"])
    require(output.returncode == 1, output.stdout + output.stderr)
    value = json.loads(output.stdout)
    require(value["truncation"]["reason"] == "error" and value["errors"] and
            not value["coverage"]["traversal_complete"], str(value))
