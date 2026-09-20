"""Log records report useful public lifecycle metadata without request secrets."""
import pytest
from logging_support import events, records
from support import eventually


@pytest.mark.parametrize("level,expected", [("off", set()), ("error", {"error"}),
    ("warning", {"error", "warning"}), ("info", {"error", "warning", "info"}),
    ("debug", {"error", "warning", "info", "debug"}),
    ("trace", {"error", "warning", "info", "debug", "trace"})])
def test_named_levels_filter_real_server_events(server_factory, tmp_path, level, expected):
    path = tmp_path / "structured.jsonl"
    server = server_factory("--log-file", path, "--log-level", level)
    server.api.run(["config", "show"])
    failed = server.api.wait(server.api.submit(["config", "show", "--invalid-option"]))
    assert failed["state"] == "failed"
    server.close()
    output = records(path)
    assert {record["level"] for record in output} <= expected
    if level == "off":
        assert not output
    else:
        assert any(record["event"] == "job.completed" and record["level"] == "error"
                   for record in output)
    if level in {"info", "debug", "trace"}:
        assert {"server.ready", "server.stopping", "server.stopped", "job.accepted",
                "job.started", "job.completed"} <= events(path)
    if level in {"debug", "trace"}:
        assert "http.response" in events(path)
    if level == "trace":
        assert {"http.read", "http.write"} <= events(path)


def test_trace_logging_never_records_client_controlled_secrets(server_factory, tmp_path):
    path = tmp_path / "private.jsonl"
    secret = "sensitive-unique-value-927184"
    server = server_factory("--log-file", path, "--log-level", "trace", token=secret)
    job = server.api.wait(server.api.submit(["config", "show", f"--{secret}"]))
    assert secret in job["stderr"]
    assert server.api.request("POST", "/v1/jobs", {secret: secret})[0] == 400
    assert server.api.request("GET", f"/unknown-{secret}?password={secret}")[0] == 404
    assert server.api.request("GET", "/health", token=f"wrong-{secret}")[0] == 401
    server.close()
    assert secret not in path.read_text()
    response = [entry for entry in records(path) if entry["event"] == "http.response"]
    assert response
    assert {entry["fields"]["status"] for entry in response} >= {200, 202, 400, 401, 404}
    assert all(entry["fields"]["duration_ms"] >= 0 for entry in response)


def test_default_foreground_logs_to_stderr(server):
    eventually(lambda: '"event":"server.ready"' in server.log.read_text())
    assert not server.config.with_name(server.config.name + ".log").exists()


def test_daemon_legacy_location_remains_supported(server_factory):
    server = server_factory("--daemon")
    path = server.config.with_name(server.config.name + ".log")
    eventually(lambda: "server.ready" in events(path))


def test_watch_logging_reports_background_activity(server_factory, tmp_path):
    path = tmp_path / "watch.jsonl"
    server = server_factory("--watch", "--log-file", path, "--log-level", "trace")
    eventually(lambda: {"watch.started", "watch.scan", "watch.scanned"} <= events(path))
    assert server.api.request("GET", "/health")[0] == 200
    server.close()
    assert "watch.stopped" in events(path)
