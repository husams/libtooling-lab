"""Global index publication is atomic and pagination detects changed snapshots."""
from urllib.parse import urlencode

from domain_http import await_symbols, completed, eventually, index_ready, submit, symbols


def refresh(api):
    job = submit(api, "/v1/extractions", {"path": "src/main.cpp", "repo": "alpha"})
    completed(api, job)


def test_malformed_and_reused_cursors_report_conflicts(domain_server):
    api = domain_server.api
    first = symbols(api, "shared::same", limit=1)
    for name, cursor in (("shared::same", "invalid"), ("alpha::answer", first["next_cursor"])):
        query = urlencode({"qualified_name": name, "cursor": cursor})
        status, error = api.request("GET", "/v1/symbols?" + query)
        assert status == 409, error
    refresh(api)
    index_ready(api)
    query = urlencode({"qualified_name": "shared::same", "cursor": first["next_cursor"]})
    status, error = api.request("GET", "/v1/symbols?" + query)
    assert status == 409, error


def test_failed_rebuild_preserves_previous_index_and_can_recover(domain_server, domain_project):
    api = domain_server.api
    facts = domain_project.facts["beta"]
    original = facts.read_bytes()
    before = symbols(api, "alpha::answer")
    try:
        facts.write_bytes(b"deliberately invalid SQLite facts for an atomic rebuild test")
        domain_project.write_source(domain_project.sources["alpha"], "alpha", "recovered")
        refresh(api)
        def failed():
            status, state = api.request("GET", "/v1/index")
            assert status == 200
            return state if state["state"] == "failed" and not state["pending"] else None
        assert eventually(failed)["error"]
        assert symbols(api, "alpha::answer") == before
        assert symbols(api, "beta::answer")["items"]
        assert not symbols(api, "alpha::recovered")["items"]
    finally:
        facts.write_bytes(original)
    refresh(api)
    index_ready(api)
    assert await_symbols(api, "alpha::recovered")["items"]
    assert not symbols(api, "alpha::answer")["items"]


def test_removed_fact_database_is_removed_from_global_search(domain_server, domain_project):
    domain_project.facts["beta"].unlink()
    refresh(domain_server.api)
    index_ready(domain_server.api)
    assert symbols(domain_server.api, "alpha::answer")["items"]
    assert not symbols(domain_server.api, "beta::answer")["items"]
