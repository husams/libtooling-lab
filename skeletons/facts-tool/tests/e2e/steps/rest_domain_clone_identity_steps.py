"""Unlabelled clones roundtrip from searchable symbols to typed operations."""
from pytest_bdd import given, then, when

from domain_http import completed, submit, symbols


@given("an indexed repository whose initial clone has no label")
def unlabelled(domain_catalog):
    domain_catalog.add("unlabelled", label=False)


@when("I extract using the clone identifier returned by symbol lookup")
def roundtrip(domain_catalog, rest_server):
    item, = symbols(rest_server.api, "unlabelled::answer")["items"]
    assert item["clone"].isdigit(), item
    job = submit(rest_server.api, "/v1/extractions", {"path": "src/main.cpp",
                 "repo": item["repo"], "clone": item["clone"]})
    domain_catalog.result = completed(rest_server.api, job)["result"]


@then("the returned clone identity resolves to the original source file")
def resolved(domain_catalog):
    assert domain_catalog.result["file"]["path"] == str(domain_catalog.sources["unlabelled"])
