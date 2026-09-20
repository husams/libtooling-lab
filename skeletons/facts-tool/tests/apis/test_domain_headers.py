"""Registered headers resolve compiler settings through their real includers."""
import pytest

from domain_headers import second_includer
from domain_http import await_symbols, completed, index_ready, submit, symbols


@pytest.mark.parametrize("operation", ["extractions", "matches", "dependencies"])
def test_header_operations_use_includer_context(domain_server, domain_project, operation):
    header = domain_project.sources["alpha"].parent / "common.hpp"
    options = {"query": 'functionDecl().bind("selected")'} if operation == "matches" else {}
    assert domain_project.rows("SELECT compile_options FROM file WHERE name='common.hpp'")[0] == (None,)
    if operation == "extractions":
        header.write_text(header.read_text() + "namespace alpha { inline int added() { return 9; } }\n")
    result = completed(domain_server.api, submit(domain_server.api, f"/v1/{operation}",
                       {"path": str(header)}, **options))["result"]
    assert result["file"]["path"] == str(header), result
    if operation == "matches":
        assert result["match_count"] == 1
        assert result["matches"][0]["bindings"]["selected"]["name"] == "alpha::helper"
        assert result["matches"][0]["bindings"]["selected"]["location"]["path"] == str(header)
    if operation == "extractions":
        assert await_symbols(domain_server.api, "alpha::added")["items"][0]["path"] == str(header)
        assert symbols(domain_server.api, "alpha::answer")["items"]


@pytest.mark.parametrize("conflicting", [False, True])
def test_header_includer_ambiguity_uses_compile_contexts(domain_project, server_factory, conflicting):
    header = second_includer(domain_project, conflicting)
    server = server_factory(*domain_project.options())
    index_ready(server.api)
    job = submit(server.api, "/v1/extractions", {"path": str(header)})
    result = server.api.wait(job["id"])
    if conflicting:
        assert result["state"] == "failed", result
        assert result["error"]["code"] == "ambiguous_compilation_context", result
    else:
        assert result["state"] == "succeeded" and result["result"]["file"]["path"] == str(header)


def test_orphan_registered_header_reports_missing_context(domain_server, domain_project):
    source = domain_project.sources["alpha"]
    source.write_text("namespace alpha { int answer() { return 9; } }\n")
    header = source.parent / "common.hpp"
    job = submit(domain_server.api, "/v1/extractions", {"path": str(header)})
    result = domain_server.api.wait(job["id"])
    assert result["state"] == "failed", result
    assert result["error"]["code"] == "compilation_context_unavailable", result


def test_inactive_clone_header_uses_its_own_source_context(domain_project, server_factory):
    source = domain_project.inactive_clone()
    header = source.parent / "common.hpp"
    active = domain_project.rows("SELECT name,active_clone_id FROM repository ORDER BY id")
    server = server_factory(*domain_project.options())
    index_ready(server.api)
    job = submit(server.api, "/v1/matches", {"path": "src/common.hpp", "repo": "alpha",
                                            "clone": "secondary"}, query='functionDecl().bind("selected")')
    result = completed(server.api, job)["result"]
    assert result["file"]["path"] == str(header) and result["match_count"] == 1
    assert result["matches"][0]["bindings"]["selected"]["location"]["path"] == str(header)
    assert domain_project.rows("SELECT name,active_clone_id FROM repository ORDER BY id") == active
