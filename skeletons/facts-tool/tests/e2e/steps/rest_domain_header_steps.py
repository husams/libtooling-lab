"""Headers resolve through actual includers and retain the requested identity."""
from pytest_bdd import given, parsers, then, when

from domain_headers import second_includer
from domain_http import completed, submit


@when(parsers.parse('I request typed "{operation}" analysis of the registered header'))
def analyse(domain_catalog, rest_server, operation):
    header = domain_catalog.sources["alpha"].parent / "common.hpp"
    options = {"query": 'functionDecl().bind("selected")'} if operation == "matches" else {}
    job = submit(rest_server.api, f"/v1/{operation}", {"path": str(header)}, **options)
    domain_catalog.header_result = completed(rest_server.api, job)["result"]


@then("the structured result identifies the requested header")
def requested_header(domain_catalog):
    result = domain_catalog.header_result
    assert result["file"]["path"] == str(domain_catalog.sources["alpha"].parent / "common.hpp")
    assert domain_catalog.rows("SELECT name,compile_options FROM file WHERE id=?",
                               (result["file"]["id"],)) == [("common.hpp", None)]


@then("header matcher results exclude declarations from the including source")
def header_only(domain_catalog):
    result = domain_catalog.header_result
    if result["operation"] == "match":
        assert result["match_count"] == 1
        assert result["matches"][0]["bindings"]["selected"]["name"] == "alpha::helper"


@given(parsers.parse('the header has two "{kind}" including compilation contexts'))
def includers(domain_catalog, kind):
    second_includer(domain_catalog, kind == "conflicting")


@when("I request header extraction with multiple including contexts")
def extract_header(domain_catalog, rest_server):
    job = submit(rest_server.api, "/v1/extractions", {"path": "src/common.hpp", "repo": "alpha"})
    domain_catalog.header_job = rest_server.api.wait(job["id"])


@then(parsers.parse('the header analysis reports "{outcome}"'))
def outcome(domain_catalog, outcome):
    job = domain_catalog.header_job
    if outcome == "succeeded":
        assert job["state"] == "succeeded", job
    else:
        assert job["state"] == "failed" and job["error"]["code"] == outcome, job


@when("I remove the header include and request typed header extraction")
def orphan(domain_catalog, rest_server):
    domain_catalog.sources["alpha"].write_text("namespace alpha { int answer() { return 9; } }\n")
    extract_header(domain_catalog, rest_server)


@when("I request matching in the inactive clone header")
def inactive_header(domain_catalog, rest_server):
    job = submit(rest_server.api, "/v1/matches", {"path": "src/common.hpp", "repo": "alpha",
        "clone": "secondary"}, query='functionDecl().bind("selected")')
    domain_catalog.header_result = completed(rest_server.api, job)["result"]


@then("matching reports the inactive header and leaves the active clone unchanged")
def inactive_result(domain_catalog):
    result = domain_catalog.header_result
    header = str(domain_catalog.inactive.parent / "common.hpp")
    assert result["file"]["path"] == header and result["match_count"] == 1
    assert result["matches"][0]["bindings"]["selected"]["location"]["path"] == header
    assert domain_catalog.rows("SELECT name,active_clone_id FROM repository ORDER BY id") == (
        domain_catalog.active_before)
