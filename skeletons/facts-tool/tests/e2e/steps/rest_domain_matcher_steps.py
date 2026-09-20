"""Client-defined matcher bindings are preserved for diverse native AST nodes."""
from pytest_bdd import parsers, then, when

from domain_http import await_symbols, completed, submit
from domain_queries import GENERIC_MATCHERS


@when(parsers.parse('I submit the "{form}" Clang DSL matcher through the typed API'))
def match(domain_catalog, rest_server, form):
    query, domain_catalog.binding_names = GENERIC_MATCHERS[form]
    job = submit(rest_server.api, "/v1/matches", {"path": "src/main.cpp", "repo": "alpha"},
                 query=query)
    domain_catalog.result = completed(rest_server.api, job)["result"]


@then("the structured match results preserve the requested bindings and AST kinds")
def bindings(domain_catalog):
    result = domain_catalog.result
    assert result["match_count"] > 0 and len(result["matches"]) == result["match_count"], result
    assert all(set(row["bindings"]) == domain_catalog.binding_names for row in result["matches"])
    assert all(binding["node_kind"] for row in result["matches"]
               for binding in row["bindings"].values())


@when(parsers.parse('I match a newly added "{declaration}" declaration without extracting'))
def added_declaration(domain_catalog, rest_server, declaration):
    source = domain_catalog.sources["alpha"]
    source.write_text(source.read_text() + "namespace fresh { using Alias = int; }\n")
    choices = {"namespace": ('namespaceDecl(hasName("fresh")).bind("space")', "fresh"),
               "alias": ('typeAliasDecl(hasName("fresh::Alias")).bind("alias")', "fresh::Alias")}
    query, domain_catalog.matched_name = choices[declaration]
    completed(rest_server.api, submit(rest_server.api, "/v1/matches",
              {"path": str(source)}, query=query))


@then("the matched declaration is available in global symbol lookup")
def searchable(domain_catalog, rest_server):
    item, = await_symbols(rest_server.api, domain_catalog.matched_name)["items"]
    assert item["path"] == str(domain_catalog.sources["alpha"])
