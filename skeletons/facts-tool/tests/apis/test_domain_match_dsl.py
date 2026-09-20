"""The REST matcher accepts Clang DSL binding names without CLI conventions."""
import sqlite3

import pytest

from domain_http import await_symbols, completed, submit
from domain_queries import GENERIC_MATCHERS


@pytest.mark.parametrize("mode", GENERIC_MATCHERS)
def test_clang_dsl_preserves_bindings_and_supported_ast_nodes(domain_server, mode):
    query, bindings = GENERIC_MATCHERS[mode]
    job = submit(domain_server.api, "/v1/matches", {"path": "src/main.cpp", "repo": "alpha"},
                 query=query)
    result = completed(domain_server.api, job)["result"]
    assert result["match_count"] > 0 and len(result["matches"]) == result["match_count"], result
    assert all(set(row["bindings"]) == bindings for row in result["matches"]), result
    assert all(binding["node_kind"] for row in result["matches"]
               for binding in row["bindings"].values())


def test_match_can_capture_source_for_arbitrarily_named_symbol(domain_server, domain_project):
    query, _ = GENERIC_MATCHERS["named"]
    job = submit(domain_server.api, "/v1/matches", {"path": "src/main.cpp", "repo": "alpha"},
                 query=query, capture_source=True)
    result = completed(domain_server.api, job)["result"]
    assert result["matches"][0]["bindings"]["chosen"]["range"]["size"] > 0
    with sqlite3.connect(domain_project.facts["alpha"].as_uri() + "?mode=ro", uri=True) as db:
        assert db.execute("SELECT count(*) FROM source_region WHERE size>0").fetchone()[0] > 0


def test_explicit_call_relation_keeps_call_and_callee_bindings(domain_server):
    query = 'callExpr(callee(functionDecl(hasName("alpha::helper")).bind("callee"))).bind("call")'
    job = submit(domain_server.api, "/v1/matches", {"path": "src/main.cpp", "repo": "alpha"},
                 query=query, relation_kind="Calls")
    result = completed(domain_server.api, job)["result"]
    assert result["match_count"] == 1, result
    row = result["matches"][0]
    assert row["relation_kind"] == "Calls" and set(row["bindings"]) == {"call", "callee"}
    assert row["bindings"]["callee"]["name"] == "alpha::helper"


@pytest.mark.parametrize("query,name", [
    ('namespaceDecl(hasName("fresh")).bind("space")', "fresh"),
    ('typeAliasDecl(hasName("fresh::Alias")).bind("alias")', "fresh::Alias")])
def test_generic_named_declarations_are_persisted_by_match(domain_server, domain_project, query, name):
    source = domain_project.sources["alpha"]
    source.write_text(source.read_text() + "namespace fresh { using Alias = int; }\n")
    completed(domain_server.api, submit(domain_server.api, "/v1/matches",
              {"path": str(source)}, query=query))
    item, = await_symbols(domain_server.api, name)["items"]
    assert item["qualified_name"] == name and item["path"] == str(source)
