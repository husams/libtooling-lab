"""Real blocking filesystem input makes cancellation deterministic without mocks."""
import os

import pytest
from pytest_bdd import then, when
from support.rest_http import eventually


@when("a native CLI job is waiting for a configuration pipe and another job is queued")
def blocking_job(rest_server):
    if not hasattr(os, "mkfifo"):
        pytest.skip("named pipe cancellation scenario requires POSIX")
    pipe = rest_server.root / "waiting.yaml"
    os.mkfifo(pipe)
    rest_server.running_id = rest_server.api.submit(["config", "show", "--config", str(pipe)])
    eventually(lambda: rest_server.api.job(rest_server.running_id)["state"] == "running")
    rest_server.queued_id = rest_server.api.submit(["config", "show"])
    assert rest_server.api.job(rest_server.queued_id)["state"] == "queued"


@then("health responds while the native command is blocked")
def responsive(rest_server):
    assert rest_server.api.request("GET", "/health")[0] == 200
    assert rest_server.api.job(rest_server.running_id)["state"] == "running"


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
