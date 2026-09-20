"""Partial work in an inactive clone must leave full extraction pending."""
from pytest_bdd import parsers, then, when

from domain_http import await_symbols, completed, index_ready, submit, symbols


@when(parsers.parse('I request partial "{operation}" analysis from the inactive clone'))
def partial(domain_catalog, rest_server, operation):
    fields = {"query": 'functionDecl(hasName("alpha::inactive")).bind("selected")'} if (
        operation == "matches") else {}
    job = submit(rest_server.api, f"/v1/{operation}",
                 {"path": "src/main.cpp", "repo": "alpha", "clone": "secondary"}, **fields)
    domain_catalog.partial = completed(rest_server.api, job)["result"]


@then("the inactive clone remains due for full extraction")
def remains_stale(domain_catalog, rest_server):
    file_id = domain_catalog.partial["file"]["id"]
    assert domain_catalog.rows("SELECT indexed FROM file WHERE id=?", (file_id,)) == [(0,)]
    index_ready(rest_server.api)
    assert not symbols(rest_server.api, "alpha::SecondaryOnly")["items"]


@then("global search includes symbols outside the partial analysis")
def full_index(domain_catalog, rest_server):
    item, = await_symbols(rest_server.api, "alpha::SecondaryOnly")["items"]
    assert item["path"] == str(domain_catalog.inactive) and item["clone"] == "secondary"
