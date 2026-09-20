"""The machine-readable schema exposes the same commands as live discovery."""
import pytest
import yaml
from openapi_spec_validator import validate


def test_openapi_paths_and_job_contract(server):
    status, schema = server.api.request("GET", "/openapi.json")
    assert status == 200
    assert schema["openapi"] == "3.1.0"
    validate(schema)
    _, catalog = server.api.request("GET", "/v1/commands")
    for command in catalog["commands"]:
        assert "post" in schema["paths"][command["endpoint"]]
    properties = schema["components"]["schemas"]["Job"]["properties"]
    assert properties["state"]["enum"] == ["queued", "running", "succeeded",
                                             "failed", "cancelled"]
    assert properties["exit_code"]["type"] == ["integer", "null"]
    assert {"get", "delete"}.issubset(schema["paths"]["/v1/jobs/{id}"])
    assert "security" not in schema


def test_openapi_authentication_contract(server_factory):
    server = server_factory(token="schema-test-secret")
    assert server.api.request("GET", "/openapi.json", token="")[0] == 401
    status, schema = server.api.request("GET", "/openapi.json")
    assert status == 200
    assert schema["security"] == [{"bearerAuth": []}]
    assert schema["components"]["securitySchemes"]["bearerAuth"]["scheme"] == "bearer"
    validate(schema)


@pytest.mark.parametrize("token", [None, "yaml-secret"])
def test_yaml_and_json_describe_the_same_live_contract(server_factory, token):
    server = server_factory(token=token)
    status, headers, content = server.api.exchange("GET", "/openapi.yaml")
    assert status == 200
    assert headers["Content-Type"].split(";")[0] == "application/yaml"
    assert len(content.splitlines()) > 20, "YAML must use readable block formatting"
    yaml_document = yaml.safe_load(content)
    validate(yaml_document)
    _, json_document = server.api.request("GET", "/openapi.json")
    assert yaml_document == json_document
    assert not any(".yaml" in value for value in references(yaml_document))
    if token:
        assert server.api.request("GET", "/openapi.yaml", token="")[0] == 401


def references(value):
    if isinstance(value, dict):
        for key, child in value.items():
            if key == "$ref":
                yield child
            else:
                yield from references(child)
    elif isinstance(value, list):
        for child in value:
            yield from references(child)
