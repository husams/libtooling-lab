from __future__ import annotations

import subprocess

from pytest_bdd import given, then
from support import callgraph_run as cg
from support.database import require
from support.scenario import FactsToolContext


def run(arguments: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(arguments, capture_output=True, text=True, check=False)


def graph(context: FactsToolContext, *extra: str) -> subprocess.CompletedProcess[str]:
    return cg.run_graph(context, "--function", "scope_fixture::a", *extra)


def node_set(context: FactsToolContext, run_id: int) -> set[str]:
    facts = context.facts_database_path
    names = {name for name, _ in cg.roots(facts, run_id)}
    for edge in cg.edges(facts, run_id):
        names.add(edge["source"])
        names.add(edge["target"])
    return names


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
    facts = context.facts_database_path
    result = graph(context)
    run_id, status = cg.completion(result)
    require(result.returncode == 0 and status == "complete", result.stdout + result.stderr)
    require(node_set(context, run_id) == {"scope_fixture::a", "scope_fixture::b",
                                          "scope_fixture::c"}, str(node_set(context, run_id)))
    row = cg.run_row(facts, run_id)
    require(row["calls_scope"] == "all" and row["components"] == "" and
            row["max_depth"] is None and row["max_nodes"] is None and
            row["max_edges"] is None and row["time_limit_ms"] is None, str(row))
    require(row["truncation_reason"] is None, str(row))
    require(not cg.frontier(facts, run_id), "expected no frontier for a complete run")


@then("explicit component and calls-scope filters expose their boundaries")
def filters(context: FactsToolContext) -> None:
    facts = context.facts_database_path
    component = graph(context, "--component", "A")
    component_id, component_status = cg.completion(component)
    require(component.returncode == 0 and component_status == "complete",
            component.stdout + component.stderr)
    require(node_set(context, component_id) == {"scope_fixture::a"},
            str(node_set(context, component_id)))
    require(not cg.edges(facts, component_id), "component filter must exclude the b edge")

    union = graph(context, "--component", "A", "--component", "C")
    union_id, _ = cg.completion(union)
    require(cg.run_row(facts, union_id)["components"] == "A,C", union_id)
    require(node_set(context, union_id) == {"scope_fixture::a"},
            str(node_set(context, union_id)))

    project = graph(context, "--calls-scope", "project")
    project_id, _ = cg.completion(project)
    require(cg.run_row(facts, project_id)["calls_scope"] == "project", project_id)
    require(len(cg.edges(facts, project_id)) == 3, str(cg.edges(facts, project_id)))

    library = graph(context, "--calls-scope", "library")
    library_id, library_status = cg.completion(library)
    require(library_status == "complete", library_status)
    require(cg.run_row(facts, library_id)["calls_scope"] == "library", library_id)
    require(not cg.edges(facts, library_id), "library scope must exclude project calls")
    library_roots = cg.roots(facts, library_id)
    require(len(library_roots) == 1 and library_roots[0][0] == "scope_fixture::a",
            "root must still be recorded even when its calls are excluded")


@then("unknown graph components fail with candidates")
def unknown_component(context: FactsToolContext) -> None:
    facts = context.facts_database_path
    before = cg.run_count(facts)
    output = graph(context, "--component", "missing")
    require(output.returncode == 2 and "unknown component 'missing'" in output.stderr and
            "candidates: A, B, C" in output.stderr, output.stdout + output.stderr)
    require(len(cg.stderr_lines(output)) == 1, output.stderr)
    require(cg.run_count(facts) == before, "usage error persisted a run")
