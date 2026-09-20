"""OpenAPI BDD drives real native jobs through generated route definitions."""
import yaml
from openapi_spec_validator import validate
from pytest_bdd import parsers, then, when
from support.rest_contract import validate_response


@when("I download the live JSON and YAML API contracts")
def download(rest_server):
    status, rest_server.schema = rest_server.api.request("GET", "/openapi.json")
    assert status == 200
    status, headers, content = rest_server.api.exchange("GET", "/openapi.yaml")
    assert status == 200 and headers["Content-Type"].split(";")[0] == "application/yaml"
    rest_server.yaml_schema = yaml.safe_load(content)


@then("both documents are valid OpenAPI and describe every registered CLI endpoint")
def contract(rest_server):
    validate(rest_server.schema)
    validate(rest_server.yaml_schema)
    assert rest_server.schema == rest_server.yaml_schema
    assert rest_server.schema["security"] == [{"bearerAuth": []}]
    _, catalog = rest_server.api.request("GET", "/v1/commands")
    assert catalog["commands"]
    for command in catalog["commands"]:
        assert "post" in rest_server.schema["paths"][command["endpoint"]]


@when(parsers.parse('I submit the configuration command through the "{path}" route'))
def submit(rest_server, path):
    _, rest_server.schema = rest_server.api.request("GET", "/openapi.json")
    status, job = rest_server.api.request("POST", f"/v1/commands/{path}", {"arguments": []})
    assert status == 202
    rest_server.responses = [("POST", "/v1/commands/{commandPath}", status, job)]
    result = rest_server.api.wait(job["id"])
    assert result["state"] == "succeeded" and "conf_root" in result["stdout"]
    rest_server.responses.append(("GET", "/v1/jobs/{id}", 200, result))


@then("its accepted response and completed job match the OpenAPI contract")
@then("every response matches its declared OpenAPI response schema")
def responses(rest_server):
    assert rest_server.responses
    for response in rest_server.responses:
        validate_response(rest_server.schema, *response)


@when("I exercise discovery, invalid requests, and a failing CLI job")
def response_examples(rest_server):
    _, rest_server.schema = rest_server.api.request("GET", "/openapi.json")
    rest_server.responses = []
    for method, path, body, expected in (
            ("GET", "/health", None, 200), ("GET", "/v1/watch", None, 200),
            ("GET", "/v1/commands", None, 200),
            ("POST", "/v1/jobs", {"arguments": []}, 400)):
        status, payload = rest_server.api.request(method, path, body)
        assert status == expected
        rest_server.responses.append((method, path, status, payload))
    failed = rest_server.api.wait(rest_server.api.submit(["config", "show", "--invalid"]))
    assert failed["state"] == "failed" and failed["exit_code"] == 2
    rest_server.responses.append(("GET", "/v1/jobs/{id}", 200, failed))
    status, listing = rest_server.api.request("GET", "/v1/jobs")
    rest_server.responses.append(("GET", "/v1/jobs", status, listing))


@then("the native CLI job is still running while metadata responds")
def still_running(rest_server):
    job = rest_server.api.job(rest_server.running_id)
    assert job["state"] == "running", job
    validate_response(rest_server.schema, "GET", "/v1/jobs/{id}", 200, job)


@then("the repository watch status conforms to the published OpenAPI schema")
def watch_schema(rest_server):
    _, document = rest_server.api.request("GET", "/openapi.json")
    status, watch = rest_server.api.request("GET", "/v1/watch")
    assert status == 200 and len(watch["clones"]) == 2
    assert any(clone["excluded"] for clone in watch["clones"])
    assert any(not clone["excluded"] for clone in watch["clones"])
    validate_response(document, "GET", "/v1/watch", status, watch)
