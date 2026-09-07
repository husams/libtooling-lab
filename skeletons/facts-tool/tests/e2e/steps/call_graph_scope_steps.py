from __future__ import annotations

import json
import subprocess
from pathlib import Path

from pytest_bdd import given, then
from support.database import require
from support.scenario import FactsToolContext


def run(arguments: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(arguments, capture_output=True, text=True, check=False)


def graph(context: FactsToolContext, *extra: str) -> subprocess.CompletedProcess[str]:
    return run([str(context.facts_tool), "analyse", "call-graph", "-v", "0",
                "-f", str(context.facts_database_path), "-c",
                str(context.files_database_path), "--format", "json",
                "--function", "scope_fixture::a", *extra])


@given("a three-component cyclic call graph is extracted")
def component_graph(context: FactsToolContext) -> None:
    context.prepare()
    roots = [context.run_root_path / name for name in ("A", "B", "C")]
    for root in roots:
        root.mkdir()
    sources = [roots[0] / "a.cpp", roots[1] / "b.cpp", roots[2] / "c.cpp"]
    sources[0].write_text(
        "namespace scope_fixture { int b(); int a(){ return b(); } }\n")
    sources[1].write_text(
        "namespace scope_fixture { int c(); int b(){ return c(); } }\n")
    sources[2].write_text(
        "namespace scope_fixture { int a(); int c(){ return a(); } int leaf(){ return 1; } }\n")
    imported = run([str(context.facts_tool), "import", "-v", "0", "-c",
                    str(context.files_database_path), "--extra-arg=-std=c++23",
                    *(f"--component={name}={root}" for name, root in
                      zip(("A", "B", "C"), roots)), *(str(path) for path in sources)])
    require(imported.returncode == 0, imported.stdout + imported.stderr)
    extracted = run([str(context.facts_tool), "extract", "-v", "0", "-o",
                     str(context.facts_database_path), "-c",
                     str(context.files_database_path), *(str(path) for path in sources)])
    require(extracted.returncode == 0, extracted.stdout + extracted.stderr)


@then("unfiltered traversal crosses all components without implicit limits")
def unfiltered(context: FactsToolContext) -> None:
    output = graph(context)
    require(output.returncode == 0, output.stdout + output.stderr)
    value = json.loads(output.stdout)
    names = {node["name"] for node in value["nodes"]}
    require(names == {"scope_fixture::a", "scope_fixture::b", "scope_fixture::c"},
            str(value))
    require(value["query"]["scope"] == {"calls": "all", "components": []},
            str(value["query"]))
    require(all(limit is None for limit in value["query"]["limits"].values()),
            str(value["query"]))
    require(value["truncation"]["reason"] is None and
            not value["excluded_scope"]["active"], str(value))


@then("explicit component and calls-scope filters expose their boundaries")
def filters(context: FactsToolContext) -> None:
    component = graph(context, "--component", "A")
    require(component.returncode == 0, component.stdout + component.stderr)
    selected = json.loads(component.stdout)
    require({node["name"] for node in selected["nodes"]} == {"scope_fixture::a"},
            str(selected))
    require(selected["excluded_scope"]["observed_edge_count"] == 1 and
            selected["excluded_scope"]["edges"][0]["reason"] == "component",
            str(selected))
    union = json.loads(graph(context, "--component", "A", "--component", "C").stdout)
    require(union["query"]["scope"]["components"] == ["A", "C"] and
            {node["name"] for node in union["nodes"]} == {"scope_fixture::a"},
            str(union))
    project = json.loads(graph(context, "--calls-scope", "project").stdout)
    require(len(project["nodes"]) == 3 and project["query"]["scope"]["calls"] == "project",
            str(project))
    library = json.loads(graph(context, "--calls-scope", "library").stdout)
    require(not library["nodes"] and library["excluded_scope"]["nodes"][0]["reason"] ==
            "calls_scope", str(library))


@then("unknown graph components fail with candidates")
def unknown_component(context: FactsToolContext) -> None:
    output = graph(context, "--component", "missing")
    require(output.returncode == 2 and "unknown component 'missing'" in output.stderr and
            "candidates: A, B, C" in output.stderr, output.stdout + output.stderr)
