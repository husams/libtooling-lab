"""Real matcher and dependency operations return typed domain results."""
from pytest_bdd import then, when

from domain_http import await_symbols, completed, submit


@when("I submit a Clang DSL query and source identity to the match API")
def match(domain_catalog, rest_server):
    source = domain_catalog.sources["alpha"]
    domain_catalog.write_source(source, "alpha", "matched")
    job = submit(rest_server.api, "/v1/matches", {"path": str(source)},
                 query='functionDecl(hasName("alpha::matched")).bind("selected")')
    domain_catalog.result = completed(rest_server.api, job)["result"]


@then("the match job returns structured bindings and the matched symbol is searchable")
def matching_results(domain_catalog, rest_server):
    result = domain_catalog.result
    assert result["operation"] == "match" and result["match_count"] == 1, result
    binding = result["matches"][0]["bindings"]["selected"]
    assert binding["name"] == "alpha::matched" and binding["usr"], binding
    assert binding["location"]["path"] == str(domain_catalog.sources["alpha"])
    item, = await_symbols(rest_server.api, "alpha::matched")["items"]
    assert item["usr"] == binding["usr"]


@when("I request dependency analysis for a repository-relative file")
def dependencies(domain_catalog, rest_server):
    job = submit(rest_server.api, "/v1/dependencies", {"path": "src/main.cpp", "repo": "beta"})
    domain_catalog.result = completed(rest_server.api, job)["result"]


@then("the dependency job returns structured source and header relationships")
def dependency_results(domain_catalog):
    result = domain_catalog.result
    assert result["operation"] == "dependencies" and result["edge_count"] >= 1, result
    names = dict(domain_catalog.rows("SELECT id,name FROM file"))
    assert any(names[edge["source_file_id"]] == "main.cpp" and
               names[edge["destination_file_id"]] == "common.hpp"
               for edge in result["edges"]), result
