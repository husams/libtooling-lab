"""Generated route patterns preserve CLI compatibility and asynchronous service."""
import sqlite3
import time
from contextlib import closing

import pytest

from contract import documented_request
from project import create_project
from support import eventually


@pytest.mark.parametrize("path", ["config/show", "config%2Fshow", "config%2fshow"])
def test_encoded_and_legacy_nested_commands(server, path):
    _, document = server.api.request("GET", "/openapi.json")
    status, submitted = documented_request(
        server, document, "POST", f"/v1/commands/{path}", {"arguments": []},
        schema_path="/v1/commands/{commandPath}")
    assert status == 202
    completed = server.api.wait(submitted["id"])
    assert completed["state"] == "succeeded"
    assert completed["arguments"][:2] == ["config", "show"]
    assert "conf_root" in completed["stdout"]


@pytest.mark.parametrize("path", ["config%", "config%2", "config%GGshow", "config%00show",
                                 "config%252Fshow", "config%3Fshow"])
def test_invalid_path_encodings_are_errors(server, path):
    assert server.api.request("POST", f"/v1/commands/{path}", {"arguments": []})[0] == 400


def test_generated_routes_are_responsive_while_cli_job_is_blocked(server, compiler):
    root = server.root / "blocked-project"
    create_project(root, compiler)
    database = root / "project.db"
    arguments = ["-p", str(root), "-c", str(database)]
    server.api.run(arguments, "import")
    _, document = server.api.request("GET", "/openapi.json")
    with closing(sqlite3.connect(database)) as lock:
        lock.execute("BEGIN EXCLUSIVE")
        status, job = documented_request(
            server, document, "POST", "/v1/commands/import", {"arguments": arguments},
            schema_path="/v1/commands/{commandPath}")
        assert status == 202
        eventually(lambda: server.api.job(job["id"])["state"] == "running")
        start = time.monotonic()
        for path in ("/health", "/v1/watch", "/v1/jobs"):
            assert documented_request(server, document, "GET", path)[0] == 200
        assert server.api.exchange("GET", "/openapi.yaml")[0] == 200
        assert time.monotonic() - start < 3
        running = server.api.job(job["id"])
        assert running["state"] == "running", running
        status, _ = documented_request(server, document, "DELETE", f'/v1/jobs/{job["id"]}',
                                       schema_path="/v1/jobs/{id}")
        assert status == 200
        assert server.api.wait(job["id"])["state"] == "cancelled"
