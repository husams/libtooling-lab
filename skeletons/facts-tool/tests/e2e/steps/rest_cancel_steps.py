"""A real database write lock keeps CLI work pending until cancellation."""
import sqlite3

import pytest
from pytest_bdd import then, when
from support.rest_http import eventually
from support.rest_project import create_project


@pytest.fixture
def locked_import(rest_server, pytestconfig):
    root = rest_server.root / "blocked-project"
    create_project(root, pytestconfig.getoption("--compiler"))
    database = root / "project.db"
    arguments = ["import", "-p", str(root), "-c", str(database)]
    rest_server.api.run(arguments)
    connection = sqlite3.connect(database)
    try:
        connection.execute("BEGIN EXCLUSIVE")
        yield arguments
    finally:
        connection.close()


@when("a native CLI job is waiting for a database lock and another job is queued")
def blocking_job(rest_server, locked_import):
    rest_server.running_id = rest_server.api.submit(locked_import)
    eventually(lambda: rest_server.api.job(rest_server.running_id)["state"] == "running")
    rest_server.queued_id = rest_server.api.submit(["config", "show"])
    assert rest_server.api.job(rest_server.queued_id)["state"] == "queued"


@then("health responds while the native command is blocked")
def responsive(rest_server):
    assert rest_server.api.request("GET", "/health")[0] == 200
    job = rest_server.api.job(rest_server.running_id)
    assert job["state"] == "running", job


@when("I cancel the queued and running REST jobs")
def cancel_jobs(rest_server):
    for identifier in (rest_server.queued_id, rest_server.running_id):
        status, body = rest_server.api.request("DELETE", f"/v1/jobs/{identifier}")
        assert status == 200, body


@then("both jobs are cancelled and the server can run another CLI command")
def cancelled(rest_server):
    for identifier in (rest_server.queued_id, rest_server.running_id):
        assert rest_server.api.wait(identifier)["state"] == "cancelled"
    queued = rest_server.api.job(rest_server.queued_id)
    assert queued.get("started_at") is None and queued["exit_code"] == -1
    assert "conf_root" in rest_server.api.run(["config", "show"])["stdout"]
