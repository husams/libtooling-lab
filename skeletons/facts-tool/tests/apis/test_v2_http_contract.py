"""V2 resource semantics validated against the live server's OpenAPI contract."""
import pytest

from contract import response_matches
from support import eventually


def checked(api, spec, method, path, body=None, schema_path=None, **options):
    status, value = api.request(method, path, body, **options)
    response_matches(spec, method, schema_path or path.split("?")[0], status, value)
    return status, value


def test_v2_discovery_settings_and_standard_methods(domain_server):
    api = domain_server.api
    _, spec = api.request("GET", "/openapi.json")
    for path in ("health", "readiness", "index", "settings", "watcher", "watcher/settings"):
        status, result = checked(api, spec, "GET", f"/api/v2/{path}")
        assert status == 200, result
    status, settings = checked(api, spec, "PATCH", "/api/v2/watcher/settings", {"debounce_ms": 125})
    assert status == 200 and settings["debounce_ms"] == 125
    status, result = checked(api, spec, "PUT", "/api/v2/watcher/settings", {"enabled": False})
    assert status == 422
    status, result = checked(api, spec, "PUT", "/api/v2/watcher/settings", settings)
    assert status == 200 and result == settings
    assert api.request("GET", "/api/v2/symbols", {"qualified_name": "alpha"})[0] == 400
    assert api.request("POST", "/api/v2/symbols", {"qualified_name": "alpha"})[0] == 405
    assert api.request("GET", "/api/v2/commands")[0] == 404


def test_v2_extraction_contract_and_publication(domain_server):
    api = domain_server.api
    _, spec = api.request("GET", "/openapi.json")
    selection = {"type": "files", "files": [{"path": "src/main.cpp", "repository": "alpha"}]}
    status, headers, raw = api.exchange("POST", "/api/v2/extract/job", {"selection": selection})
    assert status == 202, raw
    location = headers["Location"]
    assert location.startswith("/api/v2/extract/job/")
    def finished():
        status, job = checked(api, spec, "GET", location, schema_path="/api/v2/extract/job/{id}")
        assert status == 200
        return job if job["state"] in {"succeeded", "failed", "cancelled"} else None
    job = eventually(finished)
    assert job["state"] == "succeeded", job
    assert job["result"]["index_revision"]
    status, page = checked(api, spec, "GET", "/api/v2/symbols?qualified_name=alpha%3A%3Aans", schema_path="/api/v2/symbols")
    assert status == 200 and any(item["qualified_name"] == "alpha::answer" for item in page["items"])
    status, page = checked(api, spec, "GET", location + "/results?limit=1", schema_path="/api/v2/extract/job/{id}/results")
    assert status == 200 and len(page["items"]) <= 1
    status, deleted = checked(api, spec, "DELETE", location, schema_path="/api/v2/extract/job/{id}")
    assert status == 200 and deleted["state"] == "succeeded"
    assert api.request("GET", location.replace("/extract/", "/match/"))[0] == 404


@pytest.mark.parametrize("family", ["extract", "match", "dependencies", "callgraphs", "variable-flow", "scan"])
def test_v2_job_validation_rejects_cli_arguments(domain_server, family):
    api = domain_server.api
    _, spec = api.request("GET", "/openapi.json")
    status, error = checked(api, spec, "POST", f"/api/v2/{family}/job", {"arguments": ["--conf", "/tmp/db"]})
    assert status == 422 and error["error"]["code"] == "invalid_request"


def test_v2_auth_errors_are_typed(server_factory):
    server = server_factory("--no-watch", token="v2-contract-secret")
    _, spec = server.api.request("GET", "/openapi.json")
    status, error = checked(server.api, spec, "GET", "/api/v2/health", token="")
    assert status == 401 and error["error"]["code"] == "access_denied"
