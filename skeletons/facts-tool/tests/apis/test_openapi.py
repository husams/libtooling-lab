"""The machine-readable schema exposes the same commands as live discovery."""


def test_openapi_paths_and_job_contract(server):
    status, schema = server.api.request("GET", "/openapi.json")
    assert status == 200
    assert schema["openapi"] == "3.1.0"
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
