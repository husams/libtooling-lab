"""Review regressions use only ordinary CLI-produced evidence."""
import sqlite3
from pytest_bdd import given, when, then
from support.callgraph_run import completion
from support.callgraph_run_rows import edge_names as db_edge_names
from support.callgraph_run_rows import recovery as db_recovery
from support.recovery import graph, extract, success, run
from support.recovery_facts import component_tu, has_definition, is_external, query
from steps.recovery_audit_steps import audit


@then("repeated S-021 invocations reuse production facts without extraction")
def repeat(context):
    success(context.recovery_result)
    audit(context)
    for _ in range(4):
        result, run_info = graph(context)
        success(result)
        touched = [row for row in run_info["recovery"] if row[1] in ("attempted", "failed")]
        assert not touched, run_info["recovery"]
        context.recovery_run = run_info
    with sqlite3.connect(context.facts_database_path) as db:
        assert db.execute("SELECT * FROM s021_generation").fetchall() == []


@given("S-021 root also calls an unavailable standard library function")
def standard(context):
    context.recovery_sources[0].write_text(
        '#include <cstdio>\nint bridge(); int root() { std::puts("hello"); return bridge(); }\n')
    # Re-import registers the real system include paths for the changed source.
    success(run(context, "import", "--conf", context.files_database_path,
                "--facts", context.facts_database_path, "-p",
                context.recovery_sources[0].parent.parent,
                "--component", f"app={context.recovery_sources[0].parent}",
                "--component", f"library={context.recovery_sources[1].parent}"))
    success(extract(context, 0))


@then("S-021 never probes the external unavailable target")
def no_external(context):
    facts = context.facts_database_path
    run_info = context.recovery_run
    assert ("root", "puts") in run_info["edges"], run_info["edges"]
    assert not has_definition(facts, "puts")
    assert is_external(facts, "puts")
    for _, _, diagnostic in run_info["recovery"]:
        assert "puts" not in (diagnostic or ""), run_info["recovery"]


@given("the unrelated S-021 registered alternative is missing")
def missing_alternative(context):
    context.recovery_alternative.unlink()


@then("S-021 reused entries batch symbols under actual translation units")
def grouped(context):
    reused = [row for row in context.recovery_run["recovery"] if row[1] == "reused"]
    tu_ids = {row[0] for row in reused}
    assert tu_ids, reused
    for tu_id in tu_ids:
        driver, arguments = query(context.files_database_path,
                                  "SELECT driver, compile_options FROM file WHERE id=?",
                                  (tu_id,))[0]
        assert driver and arguments, (tu_id, driver, arguments)
    assert {("root", "bridge"), ("bridge", "leaf")} <= context.recovery_run["edges"]


@given("S-021 library calls a deeper missing function")
def deeper(context):
    context.recovery_sources[1].write_text("int deep(); int bridge() { return deep(); }\n")
    context.recovery_alternative.write_text("int deep() { return 5; }\n")


@when("S-021 recovery is limited to one edge")
def limited(context):
    result = run(context, "analyse", "call-graph", "-v", "0", "--conf",
                context.files_database_path, "--facts", context.facts_database_path,
                "--function", "root", "--recover-missing", "--max-depth", "1")
    success(result)
    run_id, status = completion(result)
    facts = context.facts_database_path
    context.recovery_result = result
    context.recovery_run = {"run_id": run_id, "status": status,
                            "edges": db_edge_names(facts, run_id),
                            "recovery": db_recovery(facts, run_id)}


@then("S-021 does not attempt the deeper missing function")
def depth_boundary(context):
    run_info = context.recovery_run
    assert run_info["status"] == "truncated", run_info
    assert ("bridge", "deep") not in run_info["edges"], run_info["edges"]
    library_tu = component_tu(context, "library")
    touched = {row[0]: row[1] for row in run_info["recovery"]}
    assert touched.get(library_tu) in ("attempted", "reused"), run_info["recovery"]
