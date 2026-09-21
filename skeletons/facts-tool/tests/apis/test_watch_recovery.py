"""Broken compiler inputs wait for repair instead of creating endless jobs."""
import json
import sys
import time

import pytest
from project import create_project
from support import eventually
from watch_support import symbols, wait_cycle, watch_status

pytestmark = pytest.mark.skipif(sys.platform != "linux", reason="inotify needs Linux")


@pytest.fixture
def recovery_project(server_factory, compiler, tmp_path):
    root = tmp_path / "project"
    source, _ = create_project(root, compiler)
    config = tmp_path / "defaults.yaml"
    config.write_text(f"facts_template: {root / 'facts.db'}\n")
    server = server_factory("--watch", "--conf", root / "project.db",
                            "--config", config, "--debounce-ms", "50")
    server.api.run(["-p", str(root)], "import")
    wait_cycle(server.api, 0)
    return server, root, source


def failed_cycle(api, previous):
    def failed():
        state = watch_status(api)
        return state if (state["failures"] > previous and not state["active"]
                         and not state["pending"] and not state["scanning"]) else None
    return eventually(failed)


def assert_failure_stops_retrying(api, failed):
    # Cover two one-second recovery polls; the old implementation forced a new
    # command cycle at every poll without any change to the failing inputs.
    time.sleep(2.2)
    state = watch_status(api)
    assert state["cycles"] == failed["cycles"], state
    assert state["failures"] == failed["failures"], state
    assert state["latest_jobs"] == failed["latest_jobs"], state
    assert state["last_error"] == failed["last_error"], state
    return state


def test_missing_header_waits_for_repair_and_reports_context(recovery_project):
    server, root, source = recovery_project
    previous = watch_status(server.api)["failures"]
    nested = root / "generated" / "nested"
    nested.mkdir(parents=True)
    source.write_text(source.read_text() + '\n#include "generated/nested/repaired.hpp"\n')
    failed = failed_cycle(server.api, previous)
    message = failed["last_error"]
    assert "repaired.hpp" in message, failed
    assert str(source) in message and str(root) in message, failed
    assert "job=" in message and "active_clones=" in message, failed
    assert_failure_stops_retrying(server.api, failed)
    assert server.api.request("GET", "/health")[0] == 200

    # Updating watcher settings is an explicit retry, including when the input
    # files remain unchanged. It must still stop after the repeated failure.
    status, settings = server.api.request("PATCH", "/api/v2/watcher/settings",
                                         {"debounce_ms": 51})
    assert status == 200, settings
    retried = failed_cycle(server.api, 0)
    assert retried["latest_jobs"] != failed["latest_jobs"], retried
    failed = assert_failure_stops_retrying(server.api, retried)

    (nested / "repaired.hpp").write_text("inline int watcher_repaired() { return 17; }\n")
    recovered = wait_cycle(server.api, failed["cycles"])
    assert recovered["failures"] == failed["failures"], recovered
    assert "watcher_repaired" in symbols(server.api)

    records = [json.loads(line) for line in server.log.read_text().splitlines()
               if line.startswith("{")]
    errors = [record for record in records if record["event"] == "watch.cycle.completed"
              and not record["fields"]["succeeded"]]
    assert errors and "repaired.hpp" in errors[-1]["fields"]["message"]


def test_malformed_database_waits_for_repair(recovery_project):
    server, root, _ = recovery_project
    path = root / "compile_commands.json"
    original = path.read_text()
    previous = watch_status(server.api)["failures"]
    path.write_text("not valid JSON")
    failed = failed_cycle(server.api, previous)
    # Readiness describes directory watches; compilation health is last_error.
    assert failed["ready"] and failed["last_error"], failed
    assert "compilation database" in failed["last_error"], failed
    assert str(path) in failed["last_error"], failed
    assert_failure_stops_retrying(server.api, failed)

    path.write_text(original)
    recovered = wait_cycle(server.api, failed["cycles"])
    assert recovered["failures"] == failed["failures"], recovered
    assert "main" in symbols(server.api)
