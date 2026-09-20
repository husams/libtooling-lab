"""Typed resource responses are validated against the generated OpenAPI spec."""
import pytest

from contract import response_matches
from domain_http import completed, index_ready, submit, symbols


def test_resource_query_responses_conform_to_the_live_contract(domain_server):
    api = domain_server.api
    status, document = api.request("GET", "/openapi.json")
    assert status == 200
    response_matches(document, "GET", "/v1/index", 200, index_ready(api))
    response_matches(document, "GET", "/v1/symbols", 200, symbols(api, "alpha::answer"))


@pytest.mark.parametrize("route,fields", [
    ("/v1/extractions", {}), ("/v1/dependencies", {}),
    ("/v1/matches", {"query": 'functionDecl().bind("function")'}),
])
def test_resource_operations_conform_to_the_live_contract(domain_server, route, fields):
    api = domain_server.api
    _, document = api.request("GET", "/openapi.json")
    job = submit(api, route, {"path": "src/main.cpp", "repo": "alpha"}, **fields)
    response_matches(document, "POST", route, 202, job)
    response_matches(document, "GET", "/v1/jobs/{id}", 200, completed(api, job))
    status, listing = api.request("GET", "/v1/jobs")
    response_matches(document, "GET", "/v1/jobs", status, listing)
