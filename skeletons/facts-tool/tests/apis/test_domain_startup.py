"""Startup creates managed storage or serves the previous durable snapshot."""
import sqlite3
from pathlib import Path

from domain_http import eventually, index_ready, symbols


def test_unconfigured_server_creates_its_project_index_in_background(server):
    state = index_ready(server.api)
    assert state["symbols"] == 0 and state["files"] == 0
    assert symbols(server.api, "unknown::symbol")["items"] == []
    databases = list(Path(server.env["XDG_DATA_HOME"]).rglob("*.db"))
    assert len(databases) == 1, databases
    with sqlite3.connect(databases[0].as_uri() + "?mode=ro", uri=True) as connection:
        assert connection.execute("SELECT count(*) FROM global_symbol_index").fetchone() == (0,)
    assert server.api.request("GET", "/health")[0] == 200


def test_restart_preserves_searchable_published_index_when_facts_are_corrupt(
        domain_server, domain_project, server_factory):
    previous = symbols(domain_server.api, "alpha::answer")
    domain_server.close()
    facts = domain_project.facts["beta"]
    original = facts.read_bytes()
    try:
        facts.write_bytes(b"deliberately corrupt restart fixture")
        restarted = server_factory(*domain_project.options())
        def failed():
            status, state = restarted.api.request("GET", "/v1/index")
            assert status == 200
            return state if state["state"] == "failed" and not state["pending"] else None
        assert eventually(failed)["error"]
        assert symbols(restarted.api, "alpha::answer") == previous
        assert symbols(restarted.api, "beta::answer")["items"]
        assert restarted.api.request("GET", "/health")[0] == 200
    finally:
        facts.write_bytes(original)
