from __future__ import annotations

import json
import subprocess

from pytest_bdd import given, then
from support.database import require
from support.s024_fixture import prepare
from support.scenario import FactsToolContext

def run(*command: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(command, capture_output=True, text=True, check=False)


def query(context: FactsToolContext, *args: str, conf: bool = True):
    command = [str(context.facts_tool), "analyse", "call-graph", "-v", "0",
               "-f", str(context.facts_database_path), "--format", "json"]
    if conf:
        command += ["-c", str(context.files_database_path)]
    result = run(*command, *args)
    return result, json.loads(result.stdout) if result.returncode == 0 else None


@given("the S-024 multi-component call graph corpus is extracted")
def corpus(context: FactsToolContext) -> None:
    prepare(context)


@then("S-024 forward traversal stays default and callers are explicit")
def callers(context: FactsToolContext) -> None:
    forward, graph = query(context, "--function", "s024_fixture::source")
    reverse, callers_graph = query(context, "--function", "s024_fixture::source",
                                   "--direction", "callers")
    require(forward.returncode == reverse.returncode == 0, forward.stderr + reverse.stderr)
    require(graph["query"]["mode"] == "callees" and
            "onlyCaller" not in {node["name"].split("::")[-1] for node in graph["nodes"]}, str(graph))
    require(callers_graph["query"]["mode"] == "callers" and
            any(node["name"].endswith("onlyCaller") for node in callers_graph["nodes"]), str(callers_graph))
    require(all(edge["source_usr"] and edge["target_usr"] and edge["location"]["path"]
                for edge in callers_graph["edges"]), str(callers_graph))


@then("S-024 paths cross components with deterministic cycle semantics")
def paths(context: FactsToolContext) -> None:
    args = ("--function", "s024_fixture::source", "--to", "s024_fixture::target")
    first, shortest = query(context, *args)
    second, repeated = query(context, *args)
    all_result, all_paths = query(context, *args, "--path-mode", "all-simple")
    all_repeat, _ = query(context, *args, "--path-mode", "all-simple")
    _, self_path = query(context, "--function", "s024_fixture::source", "--to",
                         "s024_fixture::source", "--path-mode", "all-simple")
    names = {node["id"]: node["name"].split("::")[-1] for node in shortest["nodes"]}
    require(first.stdout == second.stdout and shortest == repeated, "shortest output changed")
    require(all_result.stdout == all_repeat.stdout, "all-simple output changed")
    require([names[node] for node in shortest["paths"][0]["node_ids"]] ==
            ["source", "alpha", "target"], str(shortest))
    require(len(all_paths["paths"]) >= 4 and all(len(path["node_ids"]) ==
            len(set(path["node_ids"])) for path in all_paths["paths"]), str(all_paths))
    require(any(edge["certainty"] == "possible" for edge in all_paths["edges"]), str(all_paths))
    require(self_path["path_result"] == "found" and
            self_path["paths"][0]["edge_keys"] == [], str(self_path))


@then("S-024 selector and option failures are explicit usage errors")
def failures(context: FactsToolContext) -> None:
    bad = [("--function", "s024_fixture::overloaded", "--direction", "callers"),
           ("--all", "--to", "s024_fixture::target"),
           ("--function", "s024_fixture::source", "--direction", "callers",
            "--to", "s024_fixture::target"),
           ("--function", "s024_fixture::source", "--path-mode", "all-simple"),
           ("--function", "s024_fixture::source", "--to", "missing")]
    results = [query(context, *args)[0] for args in bad]
    require(all(result.returncode == 2 for result in results), str([r.stderr for r in results]))
    require("ambiguous-root" in results[0].stderr and results[0].stderr.count("usr='") >= 2,
            results[0].stderr)
    require("--to is incompatible with --all" in results[1].stderr and
            "--to is incompatible with --direction callers" in results[2].stderr and
            "--path-mode requires --to" in results[3].stderr and
            "missing-target" in results[4].stderr, str([r.stderr for r in results]))


@then("S-024 path result coverage states remain distinct")
def coverage(context: FactsToolContext) -> None:
    args = ("--function", "s024_fixture::source", "--to", "s024_fixture::isolated")
    _, complete = query(context, *args)
    _, unknown = query(context, *args, conf=False)
    _, truncated = query(context, "--function", "s024_fixture::source", "--to",
                         "s024_fixture::target", "--max-depth", "1")
    _, no_callers = query(context, "--function", "s024_fixture::isolated",
                          "--direction", "callers")
    _, unknown_callers = query(context, "--function", "s024_fixture::isolated",
                               "--direction", "callers", conf=False)
    require(complete["path_result"] == "not_found", str(complete))
    require(unknown["path_result"] == "unknown", str(unknown))
    require(truncated["path_result"] == "truncated" and truncated["truncated"] > 0,
            str(truncated))
    require(no_callers["edges"] == [] and
            no_callers["extraction_coverage"]["state"] == "complete" and
            unknown_callers["extraction_coverage"]["state"] == "unknown",
            str((no_callers, unknown_callers)))
