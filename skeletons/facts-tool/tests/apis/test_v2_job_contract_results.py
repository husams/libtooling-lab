"""Every non-graph v2 analysis exposes typed summaries and lazy record pages."""
import json

import pytest

from contract import response_matches
from support import eventually


@pytest.mark.parametrize("family,fields,collection", [
    ("match", {"expression": 'functionDecl(hasName("alpha::answer")).bind("function")'}, "matches"),
    ("dependencies", {}, "edges"),
    ("scan", {}, "warnings"),
    ("import", {"repository": "alpha"}, "databases"),
    ("index", {}, "results"),
])
def test_summary_and_record_pages_match_live_contract(
        domain_server, domain_project, family, fields, collection):
    api = domain_server.api
    _, document = api.request("GET", "/openapi.json")
    body = dict(fields)
    if family in {"match", "dependencies", "scan"}:
        body["selection"] = {"type": "repository", "repository": "alpha"}
    if family == "scan":
        (domain_project.roots["alpha"] / "broken.hpp").symlink_to("missing.hpp")
    prefix = f"/api/v2/{family}/job"
    status, headers, content = api.exchange("POST", prefix, body)
    job = json.loads(content)
    assert status == 202, job
    response_matches(document, "POST", prefix, status, job)
    location = headers.get("Location") or headers.get("location")
    assert location == prefix + "/" + job["id"]

    def terminal():
        status, snapshot = api.request("GET", location)
        assert status == 200, snapshot
        response_matches(document, "GET", prefix + "/{id}", status, snapshot)
        return snapshot if snapshot["state"] in {"succeeded", "failed", "cancelled"} else None

    job = eventually(terminal, timeout=60)
    assert job["state"] == "succeeded", job
    assert not any(isinstance(value, list) for value in job["result"].values())
    status, page = api.request("GET", location + f"/results?collection={collection}&limit=1")
    assert status == 200, page
    response_matches(document, "GET", prefix + "/{id}/results", status, page)
    assert len(page["items"]) <= 1
    if family == "scan":
        assert page["items"][0]["code"] == "broken_symlink", page
        assert job["result"]["coverage"] == "complete"
    if family == "match":
        assert page["items"][0]["bindings"]["function"]["node_kind"]
    for method, path, schema_path in [
        ("GET", prefix, prefix), ("DELETE", location, prefix + "/{id}"),
    ]:
        status, value = api.request(method, path)
        assert status == 200, value
        response_matches(document, method, schema_path, status, value)
