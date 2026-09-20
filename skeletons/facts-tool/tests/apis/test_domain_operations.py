"""Typed operations resolve repository identities and refresh the global index."""
import pytest

from domain_http import await_symbols, completed, index_ready, submit, symbols


@pytest.mark.parametrize("selection", ["absolute", "repository", "component", "clone"])
def test_extraction_resolves_file_identity(domain_server, domain_project, selection):
    source = domain_project.sources["alpha"]
    domain_project.write_source(source, "alpha", "updated")
    selectors = {"absolute": {"path": str(source)},
                 "repository": {"path": "src/main.cpp", "repo": "alpha"},
                 "component": {"path": "main.cpp", "component": "alpha-core"},
                 "clone": {"path": "src/main.cpp", "repo": "alpha", "clone": "primary-alpha"}}
    job = submit(domain_server.api, "/v1/extractions", selectors[selection])
    assert job["operation"] == "extract"
    completed(domain_server.api, job)
    await_symbols(domain_server.api, "alpha::updated")
    index_ready(domain_server.api)
    assert symbols(domain_server.api, "alpha::answer")["items"] == []
    assert symbols(domain_server.api, "beta::answer")["items"]


@pytest.mark.parametrize("selector", [{"path": "src/main.cpp"},
    {"path": "missing.cpp", "repo": "alpha"},
    {"path": "src/main.cpp", "repo": "missing"},
    {"path": "src/main.cpp", "repo": "alpha", "clone": "missing"},
    {"path": "../checkout-beta/src/main.cpp", "repo": "alpha"}])
def test_ambiguous_or_invalid_resolution_is_a_typed_failure(domain_server, selector):
    job = submit(domain_server.api, "/v1/extractions", selector)
    failure = domain_server.api.wait(job["id"])
    assert failure["state"] == "failed", failure
    assert failure["result"] is None and failure["error"]["code"], failure
    assert failure["error"]["message"], failure
    assert domain_server.api.request("GET", "/health")[0] == 200


def test_explicit_inactive_clone_does_not_switch_repository(domain_project, server_factory):
    source = domain_project.inactive_clone()
    before = domain_project.rows("SELECT name,active_clone_id FROM repository ORDER BY id")
    server = server_factory(*domain_project.options())
    index_ready(server.api)
    job = submit(server.api, "/v1/extractions", {"path": "src/main.cpp", "repo": "alpha",
                                               "clone": "secondary"})
    completed(server.api, job)
    item, = await_symbols(server.api, "alpha::inactive")["items"]
    assert item["path"] == str(source) and item["clone"] == "secondary", item
    assert domain_project.rows("SELECT name,active_clone_id FROM repository ORDER BY id") == before
