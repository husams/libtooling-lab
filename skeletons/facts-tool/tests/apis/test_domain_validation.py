"""Resource routes validate their own typed contracts and reject CLI payloads."""
import pytest


@pytest.mark.parametrize("query", ["", "?qualified_name=", "?qualified_name=x&limit=0",
    "?qualified_name=x&limit=1001", "?qualified_name=x&limit=no",
    "?qualified_name=x&conf=/tmp/project.db",
    "?qualified_name=x&facts=/tmp/facts.db", "?qualified_name=x&qualified_name=y"])
def test_invalid_symbol_queries_return_structured_errors(domain_server, query):
    status, error = domain_server.api.request("GET", "/v1/symbols" + query)
    assert status == 400, error
    assert error["error"]["code"] == "invalid_query" and error["error"]["message"], error


@pytest.mark.parametrize("route", ["extractions", "matches", "dependencies"])
@pytest.mark.parametrize("body", [None, {}, {"arguments": ["-c", "/tmp/project.db"]},
    {"file": "src/main.cpp"}, {"file": {}}, {"file": {"path": ""}},
    {"file": {"path": "src/main.cpp", "repo": 42}},
    {"file": {"path": "src/main.cpp", "unknown": "value"}},
    {"file": {"path": "src/main.cpp"}, "facts": "/tmp/facts.db"}])
def test_domain_operations_reject_malformed_requests(server, route, body):
    status, error = server.api.request("POST", f"/v1/{route}", body)
    assert status == 400, error
    assert error["error"]["code"] == "invalid_request" and error["error"]["message"], error


@pytest.mark.parametrize("body", [{"file": {"path": "x.cpp"}},
    {"file": {"path": "x.cpp"}, "query": ""},
    {"file": {"path": "x.cpp"}, "query": ["functionDecl()"]}])
def test_match_requires_a_clang_dsl_query(server, body):
    assert server.api.request("POST", "/v1/matches", body)[0] == 400


@pytest.mark.parametrize("method,path,body", [
    ("GET", "/v1/index", None), ("GET", "/v1/symbols?qualified_name=alpha", None),
    ("POST", "/v1/extractions", {"file": {"path": "src/main.cpp"}}),
    ("POST", "/v1/matches", {"file": {"path": "src/main.cpp"}, "query": "decl()"}),
    ("POST", "/v1/dependencies", {"file": {"path": "src/main.cpp"}})])
def test_domain_routes_require_authentication(server_factory, method, path, body):
    server = server_factory(token="domain-secret")
    assert server.api.request(method, path, body, token="wrong")[0] == 401
