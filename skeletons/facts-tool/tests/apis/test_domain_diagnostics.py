"""Native compiler diagnostics are typed results and cannot corrupt daemon JSONL."""
import json

import pytest

from domain_http import completed, eventually, index_ready, submit


@pytest.mark.parametrize("endpoint", ["/v1/extractions", "/v1/matches"])
def test_compiler_failures_preserve_structured_jobs_and_daemon_logs(
        domain_project, server_factory, tmp_path, endpoint):
    log = tmp_path / "diagnostics.jsonl"
    server = server_factory(*domain_project.options(), "--daemon", "--log-file", log,
                            "--log-level", "trace")
    index_ready(server.api)
    source = domain_project.sources["alpha"]
    source.write_text("int broken( { return 1; }\n")
    options = {"query": 'functionDecl().bind("selected")'} if endpoint == "/v1/matches" else {}
    job = submit(server.api, endpoint, {"path": str(source)}, **options)
    failed = server.api.wait(job["id"])
    assert failed["state"] == "failed" and failed["result"] is None, failed
    diagnostics = failed["error"]["details"]["diagnostics"]
    assert diagnostics and any(item["severity"] in {"error", "fatal"} for item in diagnostics)
    assert any(item["file"] == str(source) and item["line"] == 1 for item in diagnostics)
    assert all(item["message"] and isinstance(item["column"], int) for item in diagnostics)
    assert server.api.request("GET", "/health")[0] == 200
    def logged():
        records = [json.loads(line) for line in log.read_text().splitlines()]
        return records if any(record["event"] == "job.completed" and
                              record["fields"].get("job_id") == job["id"]
                              for record in records) else None
    assert all({"timestamp", "level", "event", "fields"}.issubset(record)
               for record in eventually(logged))


@pytest.mark.parametrize("endpoint", ["/v1/extractions", "/v1/matches"])
def test_successful_analysis_preserves_daemon_jsonl(domain_project, server_factory, tmp_path, endpoint):
    log = tmp_path / "successful.jsonl"
    server = server_factory(*domain_project.options(), "--daemon", "--log-file", log)
    index_ready(server.api)
    source = domain_project.sources["alpha"]
    domain_project.write_source(source, "alpha", "daemon")
    options = {"query": 'functionDecl(hasName("alpha::daemon")).bind("symbol")'} if (
        endpoint == "/v1/matches") else {}
    job = completed(server.api, submit(server.api, endpoint, {"path": str(source)}, **options))
    def logged():
        records = [json.loads(line) for line in log.read_text().splitlines()]
        return records if any(record["event"] == "job.completed" and
                              record["fields"].get("job_id") == job["id"]
                              for record in records) else None
    assert all({"timestamp", "level", "event", "fields"}.issubset(record)
               for record in eventually(logged))
