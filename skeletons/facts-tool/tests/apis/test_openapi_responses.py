"""Published success and error schemas agree with observable native behavior."""
import pytest

from contract import documented_request


def test_discovery_and_completed_jobs_match_contract(server):
    _, document = server.api.request("GET", "/openapi.json")
    for path in ("/health", "/v1/commands", "/v1/watch", "/v1/jobs"):
        status, _ = documented_request(server, document, "GET", path)
        assert status == 200
    status, submitted = documented_request(
        server, document, "POST", "/v1/jobs", {"arguments": ["config", "show"]})
    assert status == 202
    server.api.wait(submitted["id"])
    status, completed = documented_request(
        server, document, "GET", f'/v1/jobs/{submitted["id"]}', schema_path="/v1/jobs/{id}")
    assert status == 200 and completed["state"] == "succeeded"
    documented_request(server, document, "GET", "/v1/jobs")


@pytest.mark.parametrize("method,path,body,status,schema_path", [
    ("POST", "/v1/jobs", {"arguments": []}, 400, "/v1/jobs"),
    ("POST", "/v1/jobs", {"arguments": [1]}, 400, "/v1/jobs"),
    ("GET", "/v1/jobs/missing", None, 404, "/v1/jobs/{id}"),
    ("DELETE", "/v1/jobs/missing", None, 404, "/v1/jobs/{id}"),
    ("POST", "/v1/commands/missing", {"arguments": []}, 400,
     "/v1/commands/{commandPath}"),
])
def test_errors_match_documented_response(server, method, path, body, status, schema_path):
    _, document = server.api.request("GET", "/openapi.json")
    actual, _ = documented_request(server, document, method, path, body, schema_path)
    assert actual == status


def test_security_and_shutdown_responses_match_contract(server_factory):
    server = server_factory(token="schema-secret")
    _, document = server.api.request("GET", "/openapi.json")
    status, _ = documented_request(server, document, "GET", "/health", token="")
    assert status == 401
    status, _ = documented_request(server, document, "GET", "/health",
                                   extra_headers={"Origin": "https://example.invalid"})
    assert status == 403
    status, _ = documented_request(server, document, "POST", "/v1/shutdown", {})
    assert status == 202
