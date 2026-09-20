"""Job-list cursors bind membership while allowing lifecycle transitions."""
import sqlite3
from urllib.parse import urlencode

from support import eventually


def test_job_cursor_survives_state_changes_but_rejects_membership_changes(domain_server, domain_project):
    api = domain_server.api
    collection = "/api/v2/index/job"

    def create():
        status, job = api.request("POST", collection, {})
        assert status == 202, job
        return job["id"]

    def get(identifier):
        status, job = api.request("GET", collection + "/" + identifier)
        assert status == 200, job
        return job

    def succeeded(identifier):
        job = get(identifier)
        assert job["state"] != "failed", job
        return job if job["state"] == "succeeded" else None

    # Hold the catalog while taking the first page so its jobs are known to
    # change state before the second page is read. HTTP remains responsive.
    with sqlite3.connect(domain_project.database) as lock:
        lock.execute("BEGIN EXCLUSIVE")
        try:
            first = create()
            eventually(lambda: get(first)["state"] == "running")
            second = create()
            status, page = api.request("GET", collection + "?limit=1")
            assert status == 200, page
            assert page["items"][0]["id"] == first
            assert page["items"][0]["state"] == "running"
            assert page["next_cursor"]
            cursor = collection + "?" + urlencode({"limit": 1, "cursor": page["next_cursor"]})
        finally:
            lock.rollback()

    eventually(lambda: succeeded(first))
    eventually(lambda: succeeded(second))
    status, page = api.request("GET", cursor)
    assert status == 200, page
    assert [job["id"] for job in page["items"]] == [second]
    assert page["items"][0]["state"] == "succeeded"

    create()
    status, error = api.request("GET", cursor)
    assert status == 409, error
    assert error["error"]["code"] == "invalid_cursor", error
