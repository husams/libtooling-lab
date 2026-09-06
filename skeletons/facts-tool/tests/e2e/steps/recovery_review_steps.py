"""Review regressions use only ordinary CLI-produced evidence."""
import sqlite3
from pytest_bdd import given, when, then
from support.recovery import graph, extract, success
from steps.recovery_audit_steps import audit


@then("repeated S-021 invocations reuse production facts without extraction")
def repeat(context):
    success(context.recovery_result)
    audit(context)
    for _ in range(4):
        result, data = graph(context)
        success(result)
        assert not data["recovery"]["attempted"], data["recovery"]
        context.recovery_graph = data
    with sqlite3.connect(context.facts_database_path) as db:
        assert db.execute("SELECT * FROM s021_generation").fetchall() == []


@given("S-021 root also calls an unavailable standard library function")
def standard(context):
    context.recovery_sources[0].write_text(
        '#include <cstdio>\nint bridge(); int root() { std::puts("hello"); return bridge(); }\n')
    # Re-import registers the real system include paths for the changed source.
    from support.recovery import run
    success(run(context, "import", "--conf", context.files_database_path,
                "--facts", context.facts_database_path, "-p",
                context.recovery_sources[0].parent.parent,
                "--component", f"app={context.recovery_sources[0].parent}",
                "--component", f"library={context.recovery_sources[1].parent}"))
    success(extract(context, 0))


@then("S-021 never probes the external unavailable target")
def no_external(context):
    data = context.recovery_graph
    external = next(node for node in data["nodes"] if node["name"] == "puts")
    assert external["definition_availability"] == "external-unavailable"
    for entry in data["recovery"]["attempted"]:
        assert external["usr"] not in entry["related_usrs"], entry
    assert external["id"] in data["coverage"]["missing_definitions"]


@given("the unrelated S-021 registered alternative is missing")
def missing_alternative(context):
    context.recovery_alternative.unlink()


@then("S-021 reused entries batch symbols under actual translation units")
def grouped(context):
    entries = context.recovery_graph["recovery"]["reused"]
    ids = [entry["tu_file_id"] for entry in entries]
    assert len(ids) == len(set(ids)), entries
    assert all(entry["arguments"] and entry["driver"] for entry in entries), entries
    assert any({"c:@F@bridge#", "c:@F@leaf#"} <= set(entry["related_usrs"])
               for entry in entries), entries


@given("S-021 library calls a deeper missing function")
def deeper(context):
    context.recovery_sources[1].write_text("int deep(); int bridge() { return deep(); }\n")
    context.recovery_alternative.write_text("int deep() { return 5; }\n")


@when("S-021 recovery is limited to one edge")
def limited(context):
    import json
    from support.recovery import run
    result = run(context, "analyse", "call-graph", "-v", "0", "--conf",
                 context.files_database_path, "--facts", context.facts_database_path,
                 "--function", "root", "--format", "json", "--recover-missing",
                 "--max-depth", "1")
    success(result)
    context.recovery_graph = json.loads(result.stdout)


@then("S-021 does not attempt the deeper missing function")
def depth_boundary(context):
    attempts = context.recovery_graph["recovery"]["attempted"]
    assert any(entry["component"] == "library" for entry in attempts), attempts
    entries = attempts + context.recovery_graph["recovery"]["reused"]
    assert all("c:@F@deep#" not in entry["related_usrs"] for entry in entries), entries
