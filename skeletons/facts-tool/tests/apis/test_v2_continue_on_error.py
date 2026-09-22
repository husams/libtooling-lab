"""Failures in a batch retain diagnostics and do not hide later successful files."""
import json
from urllib.parse import quote

import pytest

from contract import response_matches
from domain_http import index_ready
from logging_support import records
from support import eventually


def completed(api, family, body):
    status, accepted = api.request("POST", f"/api/v2/{family}/job", body)
    assert status == 202, accepted
    location = f"/api/v2/{family}/job/{accepted['id']}"
    def terminal():
        status, job = api.request("GET", location)
        assert status == 200, job
        return job if job["state"] in {"succeeded", "failed", "cancelled"} else None
    return eventually(terminal, timeout=60), location


def failures(api, location):
    output, cursor = [], None
    _, document = api.request("GET", "/openapi.json")
    while True:
        url = location + "/results?collection=failed_files&limit=1"
        if cursor:
            url += "&cursor=" + quote(cursor, safe="")
        status, page = api.request("GET", url)
        assert status == 200, page
        response_matches(document, "GET", location.rsplit("/", 1)[0] + "/{id}/results", 200, page)
        output.extend(page["items"])
        cursor = page["next_cursor"]
        if cursor is None:
            return output


@pytest.mark.parametrize("family", ["extract", "match", "dependencies"])
def test_continue_multiple_failures_then_success(domain_project, server_factory, tmp_path, family):
    project = domain_project
    project.add("gamma")
    project.sources["alpha"].write_text('#include "missing-continue-header.hpp"\n')
    project.sources["beta"].unlink()  # Resolution must not discard the whole batch.
    project.write_source(project.sources["gamma"], "gamma", "after_failures")
    log = tmp_path / "continue.jsonl"
    server = server_factory(*project.options(), "--log-file", log)
    api = server.api
    index_ready(api)
    body = {"selection": {"type": "files", "files": [
        {"repository": name, "path": "src/main.cpp"} for name in ("alpha", "beta", "gamma")]},
        "continue_on_error": True}
    if family == "extract":
        body["force"] = True
    if family == "match":
        body["expression"] = 'functionDecl().bind("function")'
    job, location = completed(api, family, body)
    assert job["state"] == "succeeded", job
    result = job["result"]
    assert (result["files_selected"], result["files_processed"], result["files_failed"]) == (3, 1, 2)
    assert result["files_skipped"] == result["files_not_attempted"] == 0
    assert result["coverage"] == "partial" and result["index_revision"]
    _, document = api.request("GET", "/openapi.json")
    response_matches(document, "GET", f"/api/v2/{family}/job/{{id}}", 200, job)
    failed = failures(api, location)
    assert [f["path"] for f in failed] == [str(project.sources[n]) for n in ("alpha", "beta")]
    assert all(f["file_id"] and f["error"]["details"]["action"] for f in failed)
    details = failed[0]["error"]["details"]
    assert details["compilation_commands"] and details["effective_compilation_commands"]
    assert details["diagnostics"] and details["request"]["continue_on_error"]
    if family == "extract":
        status, symbols = api.request("GET", "/api/v2/symbols?qualified_name=gamma%3A%3Aafter_failures&match=exact")
        assert status == 200 and symbols["items"], symbols
    def logged():
        return [r for r in records(log) if r["event"] == "job.file_failed" and r["fields"]["job_id"] == job["id"]]
    assert len(eventually(lambda: logged() if len(logged()) == 2 else None)) == 2
    contexts = [r["fields"] for r in records(log) if r["event"] == "job.context"
                and r["fields"]["job_id"] == job["id"] and r["fields"]["file_id"] == failed[0]["file_id"]
                and r["fields"]["key"] == "effective_compilation_commands"]
    assert json.loads("".join(p["value"] for p in sorted(contexts, key=lambda p: p["part"]))) == details["effective_compilation_commands"]
    # A partial success can be retried after repair; the original remains unchanged.
    for name in ("alpha", "beta"):
        project.write_source(project.sources[name], name, "repaired")
    retried, _ = completed(api, family, {"retry_of": job["id"]})
    assert retried["state"] == "succeeded", retried
    assert retried["retry_of"] == job["id"] and retried["id"] != job["id"]
    assert retried["result"]["files_failed"] == 0
    assert retried["result"]["coverage"] == "complete"
    assert failures(api, location) == failed


def test_default_stops_and_retains_failure_results(domain_project, domain_server):
    project = domain_project
    project.sources["alpha"].write_text('#include "missing-default-header.hpp"\n')
    job, location = completed(domain_server.api, "extract", {
        "selection": {"type": "files", "files": [
            {"repository": name, "path": "src/main.cpp"} for name in ("alpha", "beta")]}})
    assert job["state"] == "failed", job
    assert job["result"]["files_processed"] == 0
    assert job["result"]["files_failed"] == job["result"]["files_not_attempted"] == 1
    assert len(failures(domain_server.api, location)) == 1
    assert job["error"]["details"]["diagnostics"]


@pytest.mark.parametrize("family", ["extract", "match", "dependencies"])
def test_continue_flag_requires_boolean(server, family):
    body = {"selection": {"type": "all"}, "continue_on_error": "true"}
    if family == "match":
        body["expression"] = 'functionDecl()'
    status, error = server.api.request("POST", f"/api/v2/{family}/job", body)
    assert status == 422 and error["error"]["code"] == "invalid_request"


def test_every_file_failed_is_failed_with_complete_collection(domain_project, domain_server):
    for source in domain_project.sources.values():
        source.write_text('#include "all-files-missing.hpp"\n')
    job, location = completed(domain_server.api, "extract", {
        "selection": {"type": "all"}, "continue_on_error": True})
    assert job["state"] == "failed" and job["error"]["code"] == "analysis_failed"
    assert job["result"]["files_failed"] == job["result"]["files_selected"] == 2
    assert job["result"]["files_not_attempted"] == job["result"]["files_processed"] == 0
    assert len(failures(domain_server.api, location)) == 2
