"""Review regressions through real native commands and recovery signals."""
import json
import signal
import subprocess
import threading
from pytest_bdd import when, then, parsers
from support.graph_artifact import command, metadata
from support.recovery import run, edge_names


@when(parsers.parse("S-025 interrupts {format} recovery during extraction"))
def interrupt_recovery(context, format):
    source = context.recovery_sources[1]
    source.write_text(source.read_text() + "\n".join(
        f"struct S025ReviewType{i} {{ int field; }};" for i in range(15000)))
    context.graph_interrupt_initial = None
    captured = []
    args = command(context, recover=True, format=format, extra=("--verbose", "3"))
    with subprocess.Popen([str(context.facts_tool), *args],
                          env=context.recovery_env, stdout=subprocess.PIPE,
                          stderr=subprocess.PIPE, text=True) as process:
        def drain():
            for line in process.stderr:
                captured.append(line)
                if "recovery validation tu=" in line:
                    if context.graph_artifact.exists():
                        context.graph_interrupt_initial = metadata(context.graph_artifact)
                    process.send_signal(signal.SIGINT)
        reader = threading.Thread(target=drain)
        reader.start()
        try:
            context.graph_returncode = process.wait(timeout=90)
        finally:
            if process.poll() is None:
                process.kill()
            reader.join(timeout=10)
        context.graph_streams = (process.stdout.read(), "".join(captured))


@then(parsers.parse("S-025 retains a useful cancelled {format} result"))
def cancelled_result(context, format):
    assert context.graph_returncode == 130, context.graph_streams
    text = context.graph_artifact.read_text()
    graph = metadata(context.graph_artifact) if format == "mermaid" else json.loads(text)
    assert not graph["complete"] and graph["truncation"]["reason"] == "cancelled", graph
    assert ("root", "bridge") in edge_names(graph), graph
    assert graph["selected_roots"][0]["name"] == "root"
    assert graph["artifact_stage"] == "final"
    if format == "mermaid":
        assert context.graph_interrupt_initial is not None
        assert edge_names(context.graph_interrupt_initial) <= edge_names(graph)
        assert "Partial graph" in text and " -->|" in text
    assert not list(context.graph_artifact.parent.glob("graph.mmd.tmp-*"))


@when("S-025 requests JSON from an empty facts database")
def json_error(context):
    empty = context.run_root_path / "empty-review.db"
    empty.touch()
    context.graph_result = run(context, "analyse", "call-graph", "--facts", empty,
                               "--all", "--format", "json")


@then("S-025 emits one JSON error without a duplicate stderr diagnostic")
def json_error_result(context):
    result = context.graph_result
    assert result.returncode == 1
    assert json.loads(result.stdout)["errors"]
    assert result.stderr == ""
