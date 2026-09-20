"""Queued native operations cancel without interrupting in-flight transactions."""
import sqlite3

from domain_http import completed, eventually, submit


def test_cancel_queued_native_work_and_preserve_running_transaction(domain_server, domain_project):
    api = domain_server.api
    with sqlite3.connect(domain_project.database) as lock:
        lock.execute("BEGIN EXCLUSIVE")
        try:
            running = submit(api, "/v1/extractions", {"path": "src/main.cpp", "repo": "alpha"})
            eventually(lambda: api.job(running["id"])["state"] == "running")
            queued = submit(api, "/v1/dependencies", {"path": "src/main.cpp", "repo": "beta"})
            status, error = api.request("DELETE", f"/v1/jobs/{running['id']}")
            assert status == 409 and error["error"]["code"] == "cannot_cancel_running", error
            status, cancelled = api.request("DELETE", f"/v1/jobs/{queued['id']}")
            assert status == 200 and cancelled["state"] == "cancelled", cancelled
            assert cancelled["result"] is None and cancelled["error"] is None
            assert api.request("GET", "/health")[0] == 200
        finally:
            lock.rollback()
    completed(api, running)
    assert api.job(queued["id"])["state"] == "cancelled"
