"""Portable graph artifacts use real native extraction and recovery."""
import json
from pytest_bdd import given, when, then, parsers
from support.recovery import prepare, success, edge_names
from support.graph_artifact import invoke, metadata, database_bytes


@given("the S-025 two-component graph fixture")
def prepare_graph(context):
    prepare(context)
    context.graph_artifact = context.run_root_path / "graph.mmd"


@when("S-025 recovers a Mermaid artifact")
def recover_graph(context):
    invoke(context, recover=True)


@then("the S-025 artifact contains the recovered graph and provenance")
def recovered_artifact(context):
    success(context.graph_result)
    data = metadata(context.graph_artifact)
    assert {("root", "bridge"), ("bridge", "leaf")} <= edge_names(data)
    assert data["provenance"]["facts"] == str(context.facts_database_path.resolve())
    assert data["roots"][0]["usr"] and data["artifact_stage"] == "final"
    assert data["query"]["scope"] == {"calls": "all", "components": []}
    assert all(value is None for value in data["query"]["limits"].values())
    assert data["recovery"]["requested"] and not data["errors"]
    assert all(isinstance(node["id"], str) for node in data["nodes"])
    assert all(edge["location"] for edge in data["edges"])


@then("S-025 read-only output reuses that graph without changing its databases")
def read_only(context):
    before = database_bytes(context)
    success(invoke(context))
    assert before == database_bytes(context)
    assert "recovery-start" not in context.graph_result.stderr
    assert ("bridge", "leaf") in edge_names(metadata(context.graph_artifact))


@given("the S-025 library fails compilation")
def broken_library(context):
    context.recovery_sources[1].write_text("#error S025_RECOVERY_FAILURE\n")


@then("S-025 retains a valid partial artifact with a recovery error")
def failure_artifact(context):
    assert context.graph_result.returncode == 1, context.graph_result
    data = metadata(context.graph_artifact)
    assert data["recovery"]["failed"] and data["errors"]
    assert ("root", "bridge") in edge_names(data)
    assert ("bridge", "leaf") not in edge_names(data)
    assert "initial graph published" in context.graph_result.stderr
    assert "[unresolved project definition]" in context.graph_artifact.read_text()


@when(parsers.parse("S-025 writes a {format} graph artifact"))
def output_format(context, format):
    invoke(context, format=format, recover=True)


@then(parsers.parse("S-025 has a coherent {format} file and empty stdout"))
def coherent_output(context, format):
    success(context.graph_result)
    assert context.graph_result.stdout == ""
    text = context.graph_artifact.read_text()
    if format == "json":
        assert json.loads(text)["recovery"]["requested"]
    elif format == "mermaid":
        assert metadata(context.graph_artifact)["artifact_stage"] == "final"
    else:
        assert text.count("root=root usr=") == 1
        assert "recovery-requested=true" in text
    assert not list(context.graph_artifact.parent.glob("graph.mmd.tmp-*"))
