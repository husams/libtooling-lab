"""Recovery behavior exercised exclusively through the native command."""
from pytest_bdd import given, when, then
from support.recovery import prepare, graph, edge_names, extract, seed_match, success, mark_complete


@given("an S-021 app calls an unextracted registered library")
def fixture(context):
    prepare(context)


@given("the S-021 library has only symbol-match evidence")
def symbol_only(context):
    seed_match(context)


@given("the S-021 library already has valid call facts")
def complete(context):
    success(extract(context, 1))
    mark_complete(context, "library.cpp")


@when("S-021 missing recovery is requested")
def recover(context):
    context.recovery_result, context.recovery_graph = graph(context)


@then("S-021 recovers the library body with its stored command")
def recovered(context):
    result, data = context.recovery_result, context.recovery_graph
    success(result)
    assert data["schema_version"] == 1 and data["errors"] == [], data
    assert {("root", "bridge"), ("bridge", "leaf")} <= edge_names(data), data
    recovery = data["recovery"]
    assert recovery["requested"] is True
    assert not recovery["failed"], recovery
    attempts = recovery["attempted"]
    library = [entry for entry in attempts if entry["component"] == "library"]
    assert library, recovery
    assert all("-DS021_LIBRARY=1" in entry["arguments"] for entry in library), library
    assert all(entry["driver"] and isinstance(entry["tu_file_id"], str) and
               entry["tu_file_id"].isdigit() for entry in library), library
    expected_cwd = str(context.recovery_sources[1].parent)
    assert all(entry["working_directory"] == expected_cwd for entry in library), (
        expected_cwd, [entry["working_directory"] for entry in library])
    assert "recovery-start" in result.stderr and "recovery-complete" in result.stderr


@then("S-021 reuses existing facts without extraction")
def reused(context):
    success(context.recovery_result)
    recovery = context.recovery_graph["recovery"]
    assert recovery["attempted"] == [], recovery
    assert recovery["reused"], recovery
    assert ("bridge", "leaf") in edge_names(context.recovery_graph)


@then("omitting S-021 recovery preserves the partial graph and stores")
def readonly(context):
    before = [path.read_bytes() for path in
              (context.files_database_path, context.facts_database_path)]
    result, data = graph(context, recover=False)
    success(result)
    assert ("root", "bridge") in edge_names(data)
    assert ("bridge", "leaf") not in edge_names(data)
    assert before == [path.read_bytes() for path in
                      (context.files_database_path, context.facts_database_path)]
    assert "recovery-start" not in result.stderr
