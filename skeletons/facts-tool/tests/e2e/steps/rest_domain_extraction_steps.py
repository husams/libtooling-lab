"""Typed extraction requests select registered sources without database paths."""
from pytest_bdd import parsers, then, when

from domain_http import await_symbols, completed, index_ready, submit, symbols


@when(parsers.parse('I request typed extraction using the "{selector}" file identity'))
def extract(domain_catalog, rest_server, selector):
    source = domain_catalog.sources["alpha"]
    domain_catalog.write_source(source, "alpha", "updated")
    selectors = {"absolute": {"path": str(source)},
                 "repository": {"path": "src/main.cpp", "repo": "alpha"},
                 "component": {"path": "main.cpp", "component": "alpha-core"},
                 "clone": {"path": "src/main.cpp", "repo": "alpha", "clone": "primary-alpha"}}
    domain_catalog.job = submit(rest_server.api, "/v1/extractions", selectors[selector])


@then("the extraction job returns structured results without command output")
def structured_extraction(domain_catalog, rest_server):
    job = completed(rest_server.api, domain_catalog.job)
    assert job["operation"] == "extract"
    assert job["result"]["facts_committed"] is True and job["result"]["symbol_count"] > 0
    assert job["result"]["file"]["path"] == str(domain_catalog.sources["alpha"])


@then("global search contains the new symbol and removes the replaced symbol")
def refreshed(rest_server):
    assert await_symbols(rest_server.api, "alpha::updated")["items"]
    index_ready(rest_server.api)
    assert not symbols(rest_server.api, "alpha::answer")["items"]
    assert symbols(rest_server.api, "beta::answer")["items"]


@when("I request extraction for a relative file shared by both repositories")
def ambiguous(domain_catalog, rest_server):
    domain_catalog.job = submit(rest_server.api, "/v1/extractions", {"path": "src/main.cpp"})


@then("the job fails with a typed ambiguity error and the server remains healthy")
def ambiguity_error(domain_catalog, rest_server):
    job = rest_server.api.wait(domain_catalog.job["id"])
    assert job["state"] == "failed" and job["result"] is None, job
    assert "ambig" in job["error"]["code"].lower(), job
    assert job["error"]["message"] and rest_server.api.request("GET", "/health")[0] == 200


@when("I request extraction from the explicitly selected inactive clone")
def inactive(domain_catalog, rest_server):
    job = submit(rest_server.api, "/v1/extractions", {"path": "src/main.cpp", "repo": "alpha",
                                                   "clone": "secondary"})
    domain_catalog.result = completed(rest_server.api, job)


@then("global search reports the inactive clone definition and the active clone is unchanged")
def inactive_unchanged(domain_catalog, rest_server):
    item, = await_symbols(rest_server.api, "alpha::inactive")["items"]
    assert item["path"] == str(domain_catalog.inactive) and item["clone"] == "secondary"
    assert domain_catalog.rows("SELECT name,active_clone_id FROM repository ORDER BY id") == (
        domain_catalog.active_before)
