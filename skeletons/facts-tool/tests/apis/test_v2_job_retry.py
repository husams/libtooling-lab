"""Failed/cancelled jobs can be retried without losing their original history."""
import json
import sqlite3

import pytest
from jsonschema import Draft202012Validator

from contract import response_matches
from support import eventually


def terminal(api, collection, identifier):
    def finished():
        status, job = api.request("GET", collection + "/" + identifier)
        assert status == 200, job
        return job if job["state"] in {"succeeded", "failed", "cancelled"} else None

    return eventually(finished, timeout=60)


def test_failed_import_retries_retained_request_after_database_is_fixed(
        domain_server, domain_project):
    api = domain_server.api
    collection = "/api/v2/import/job"
    database = domain_project.roots["alpha"] / "compile_commands.json"
    original = database.read_text()
    database.write_text("{broken json")
    status, failed = api.request("POST", collection, {
        "repository": "alpha", "compilation_database": str(database)})
    assert status == 202, failed
    failed = terminal(api, collection, failed["id"])
    assert failed["state"] == "failed", failed

    # Repair the original input; the server owns the retained import request.
    database.write_text(original)
    body = {"retry_of": failed["id"]}
    _, document = api.request("GET", "/openapi.json")
    request_schema = document["paths"][collection]["post"]["requestBody"]["content"]["application/json"]["schema"]
    Draft202012Validator({"components": document["components"], **request_schema}).validate(body)
    status, headers, content = api.exchange("POST", collection, body)
    retried = json.loads(content)
    assert status == 202, retried
    assert retried["id"] != failed["id"]
    assert retried["retry_of"] == failed["id"]
    assert (headers.get("Location") or headers.get("location")) == collection + "/" + retried["id"]
    response_matches(document, "POST", collection, status, retried)
    retried = terminal(api, collection, retried["id"])
    assert retried["state"] == "succeeded", retried
    assert retried["retry_of"] == failed["id"]
    response_matches(document, "GET", collection + "/{id}", 200, retried)
    assert terminal(api, collection, failed["id"]) == failed

    for target, identifier, expected, code in [
        (collection, retried["id"], 409, "job_not_retryable"),
        ("/api/v2/extract/job", failed["id"], 404, "job_not_found"),
        (collection, "unknown-job", 404, "job_not_found"),
    ]:
        status, error = api.request("POST", target, {"retry_of": identifier})
        assert status == expected, error
        assert error["error"]["code"] == code, error


def test_cancelled_job_retries_with_fresh_cancellation_state_and_rejects_active_jobs(
        domain_server, domain_project):
    api = domain_server.api
    collection = "/api/v2/index/job"

    # Keep the worker occupied so admission and cancellation are deterministic.
    with sqlite3.connect(domain_project.database) as lock:
        lock.execute("BEGIN EXCLUSIVE")
        try:
            status, first = api.request("POST", collection, {})
            assert status == 202, first
            eventually(lambda: api.request("GET", collection + "/" + first["id"])[1]["state"] == "running")
            status, cancelled = api.request("POST", collection, {})
            assert status == 202, cancelled
            for identifier in [first["id"], cancelled["id"]]:
                status, error = api.request("POST", collection, {"retry_of": identifier})
                assert status == 409, error
                assert error["error"]["code"] == "job_not_retryable"
            status, cancelled = api.request("DELETE", collection + "/" + cancelled["id"])
            assert status == 200 and cancelled["state"] == "cancelled", cancelled
            status, retried = api.request("POST", collection, {"retry_of": cancelled["id"]})
            assert status == 202, retried
            assert retried["retry_of"] == cancelled["id"]
        finally:
            lock.rollback()

    assert terminal(api, collection, first["id"])["state"] == "succeeded"
    assert terminal(api, collection, retried["id"])["state"] == "succeeded"
    assert terminal(api, collection, cancelled["id"]) == cancelled


@pytest.mark.parametrize("body", [
    {"retry_of": ""}, {"retry_of": None}, {"retry_of": 1},
    {"retry_of": "a\x00b"}, {"retry_of": "x" * 4097},
    {"retry_of": "d1", "force": True},
])
def test_retry_rejects_malformed_identity_and_request_overrides(server, body):
    status, error = server.api.request("POST", "/api/v2/extract/job", body)
    assert status == 400, error
    assert error["error"]["code"] == "invalid_request"
