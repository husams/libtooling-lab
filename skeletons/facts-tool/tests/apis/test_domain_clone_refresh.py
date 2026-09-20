"""Partial work in another clone must not make full extraction falsely fresh."""
import pytest

from domain_http import await_symbols, completed, index_ready, submit, symbols


@pytest.mark.parametrize("endpoint", ["/v1/matches", "/v1/dependencies"])
def test_partial_inactive_clone_work_requires_full_extraction(
        domain_project, server_factory, endpoint):
    source = domain_project.inactive_clone()
    source.write_text(source.read_text() + "namespace alpha { class SecondaryOnly {}; }\n")
    active = domain_project.rows("SELECT name,active_clone_id FROM repository ORDER BY id")
    server = server_factory(*domain_project.options())
    index_ready(server.api)
    selector = {"path": "src/main.cpp", "repo": "alpha", "clone": "secondary"}
    fields = {"query": 'functionDecl(hasName("alpha::inactive")).bind("selected")'} if (
        endpoint == "/v1/matches") else {}
    partial = completed(server.api, submit(server.api, endpoint, selector, **fields))["result"]
    if endpoint == "/v1/dependencies":
        assert any(edge["source_path"] == str(source) and edge["destination_path"] == str(
            source.parent / "common.hpp") for edge in partial["edges"]), partial
    assert domain_project.rows("SELECT indexed FROM file WHERE id=?", (partial["file"]["id"],)) == [(0,)]
    index_ready(server.api)
    assert not symbols(server.api, "alpha::SecondaryOnly")["items"]
    completed(server.api, submit(server.api, "/v1/extractions", selector))
    item, = await_symbols(server.api, "alpha::SecondaryOnly")["items"]
    assert item["path"] == str(source) and item["clone"] == "secondary"
    assert domain_project.rows("SELECT name,active_clone_id FROM repository ORDER BY id") == active
