"""Native cancellation leaves an active database transaction to finish safely."""
import sqlite3

from pytest_bdd import then, when

from domain_http import completed, eventually, submit


@when("I cancel queued domain work while another operation is waiting for the database")
def cancel(domain_catalog, rest_server):
    api = rest_server.api
    with sqlite3.connect(domain_catalog.database) as lock:
        lock.execute("BEGIN EXCLUSIVE")
        try:
            running = submit(api, "/v1/extractions", {"path": "src/main.cpp", "repo": "alpha"})
            eventually(lambda: api.job(running["id"])["state"] == "running")
            queued = submit(api, "/v1/dependencies", {"path": "src/main.cpp", "repo": "beta"})
            domain_catalog.running_cancel = api.request("DELETE", f"/v1/jobs/{running['id']}")
            domain_catalog.queued_cancel = api.request("DELETE", f"/v1/jobs/{queued['id']}")
            domain_catalog.running_job, domain_catalog.queued_job = running, queued
            assert api.request("GET", "/health")[0] == 200
        finally:
            lock.rollback()


@then("only the queued domain operation is cancelled and the running operation completes")
def cancelled(domain_catalog, rest_server):
    status, error = domain_catalog.running_cancel
    assert status == 409 and error["error"]["code"] == "cannot_cancel_running", error
    status, job = domain_catalog.queued_cancel
    assert status == 200 and job["state"] == "cancelled", job
    completed(rest_server.api, domain_catalog.running_job)
    assert rest_server.api.job(domain_catalog.queued_job["id"])["state"] == "cancelled"
