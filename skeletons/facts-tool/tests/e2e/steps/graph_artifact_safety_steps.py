"""Invalid graph output never replaces its inputs or existing artifacts."""
import os

from pytest_bdd import when, then, parsers
from support.recovery import run, success
from support.graph_artifact import command, invoke, metadata


@when(parsers.parse("S-025 requests an invalid {kind} graph artifact"))
def invalid_request(context, kind):
    context.graph_artifact.write_text("previous artifact\n")
    args = command(context, recover=True)
    if kind == "root":
        args[args.index("--function") + 1] = "missing-root"
    elif kind == "target":
        args += ["--to", "missing-target"]
    elif kind == "configuration":
        args[args.index("--conf") + 1] = str(context.run_root_path / "missing.db")
    else:
        args += ["--component", "missing-component"]
    context.graph_result = run(context, *args)


@then("S-025 rejects the request without replacing the artifact")
def invalid_preserved(context):
    assert context.graph_result.returncode != 0
    assert context.graph_artifact.read_text() == "previous artifact\n"
    assert "recovery-start" not in context.graph_result.stderr


@when(parsers.parse("S-025 writes over the {input} input"))
def overlap(context, input):
    paths = {"facts": context.facts_database_path,
             "project": context.files_database_path,
             "source": context.recovery_sources[0]}
    context.protected_input = paths.get(input, context.facts_database_path)
    context.protected_bytes = context.protected_input.read_bytes()
    if input == "alias":
        alias = context.run_root_path / "facts-alias"
        # os.link: Path.hardlink_to needs Python 3.10, RHEL 9 ships 3.9.
        os.link(context.protected_input, alias)
        context.graph_artifact = alias
    else:
        context.graph_artifact = context.protected_input
    invoke(context, recover=True)


@then("S-025 rejects the output and preserves that input")
def input_preserved(context):
    assert context.graph_result.returncode == 2, context.graph_result.stderr
    assert context.protected_input.read_bytes() == context.protected_bytes
    assert "recovery-start" not in context.graph_result.stderr


@when("S-025 renders with a one-node budget")
def budget(context):
    success(invoke(context, extra=("--max-nodes", "1")))


@then("S-025 renders the truncation frontier without claiming completion")
def truncated(context):
    data = metadata(context.graph_artifact)
    assert not data["complete"] and not data["coverage"]["traversal_complete"]
    assert data["truncation"]["reason"] == "max_nodes"
    assert data["truncation"]["frontier"]
    assert len(data["nodes"]) == 1


@when(parsers.re(r"S-025 renders a (?P<mode>callers|path) query"))
def optional_mode(context, mode):
    success(invoke(context, recover=True))
    extra = ("--direction", "callers") if mode == "callers" else ("--to", "leaf")
    success(invoke(context, extra=extra))


@then(parsers.parse("S-025 retains the {mode} query metadata"))
def query_metadata(context, mode):
    data = metadata(context.graph_artifact)
    assert data["query"]["mode"] == mode
    if mode == "path":
        assert data["path_result"] == "found" and data["paths"]
