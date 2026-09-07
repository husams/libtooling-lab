"""Observe the real artifact while a native recovery subprocess is running."""
import json
import subprocess
import threading
import time
from pytest_bdd import when, then
from support.graph_artifact import command, metadata


@when("S-025 observes the artifact during recovery")
def live_artifact(context):
    # Real additional source work makes the pre-recovery publication observable.
    source = context.recovery_sources[1]
    source.write_text(source.read_text() + "\n".join(
        f"struct S025Type{i} {{ int field; }};" for i in range(12000)))
    observed = []
    with subprocess.Popen([str(context.facts_tool), *command(context, recover=True)],
                          env=context.recovery_env, stdout=subprocess.PIPE,
                          stderr=subprocess.PIPE, text=True) as process:
        captured = {}
        def drain():
            captured["streams"] = process.communicate(timeout=90)
        reader = threading.Thread(target=drain)
        reader.start()
        while reader.is_alive():
            if context.graph_artifact.exists():
                text = context.graph_artifact.read_text()
                rows = [line[3:] for line in text.splitlines()
                        if line.startswith("%% {")]
                assert len(rows) == 1 and "flowchart TD" in text, text
                observed.append(json.loads(rows[0]))
            time.sleep(0.001)
        reader.join()
        context.graph_streams = captured["streams"]
        context.graph_returncode = process.returncode
    context.graph_observed = observed


@then("S-025 observed a labelled initial graph and its final replacement")
def live_result(context):
    assert context.graph_returncode == 0, context.graph_streams
    initial = [item for item in context.graph_observed
               if item["artifact_stage"] == "initial"]
    assert initial and all(item["recovery_pending"] for item in initial)
    assert metadata(context.graph_artifact)["artifact_stage"] == "final"
    assert context.graph_streams[0] == ""
    assert not list(context.graph_artifact.parent.glob("graph.mmd.tmp-*"))
