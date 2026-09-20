"""Authentication and discoverable parity with the installed CLI."""
import re
import subprocess

from pytest_bdd import then, when


@when("requests omit the token or supply the wrong token")
def rejected_requests(rest_server):
    rest_server.denied = [rest_server.api.request(method, path, body, token=token)[0]
                         for token in ("", "wrong-secret")
                         for method, path, body in (
                             ("GET", "/v1/commands", None),
                             ("GET", "/openapi.json", None),
                             ("GET", "/v1/jobs", None),
                             ("GET", "/v1/watch", None),
                             ("POST", "/v1/jobs", {"arguments": ["config", "show"]}),
                             ("POST", "/v1/shutdown", {}))]


@then("every protected request is unauthorized and the server stays healthy")
def rejected_and_healthy(rest_server):
    assert rest_server.denied and set(rest_server.denied) == {401}
    assert rest_server.api.request("GET", "/health")[0] == 200


@when("I discover the REST command catalog and OpenAPI document")
def discover(rest_server):
    status, catalog = rest_server.api.request("GET", "/v1/commands")
    assert status == 200
    rest_server.catalog = {item["path"]: item["endpoint"] for item in catalog["commands"]}
    status, rest_server.schema = rest_server.api.request("GET", "/openapi.json")
    assert status == 200


@then("every installed CLI command has a working REST help endpoint")
def command_parity(rest_server):
    pending, discovered = [[]], set()
    while pending:
        path = pending.pop()
        result = subprocess.run([str(rest_server.executable), *path, "--help"],
                                cwd=rest_server.root, env=rest_server.environment,
                                capture_output=True, text=True, timeout=10, check=False)
        assert result.returncode == 0, result.stderr
        sections = re.split("subcommands:", result.stdout, flags=re.IGNORECASE)
        children = re.findall(r"^  ([a-z][a-z0-9-]*)(?:,\s*[a-z][a-z0-9-]*)*\s{2,}",
                              sections[1], re.MULTILINE) if len(sections) > 1 else []
        for child in children:
            if child != "serve":
                pending.append([*path, child])
                discovered.add("/".join([*path, child]))
    assert discovered and discovered.issubset(rest_server.catalog)
    assert "serve" not in rest_server.catalog
    for path, endpoint in rest_server.catalog.items():
        assert endpoint == f"/v1/commands/{path}"
        job = rest_server.api.run(["--help"], path)
        assert "facts-tool" in job["stdout"] and "--help" in job["stdout"]


@then("OpenAPI describes all commands, authenticated requests, and job states")
def openapi_contract(rest_server):
    schema = rest_server.schema
    assert schema["openapi"] == "3.1.0"
    assert schema["security"] == [{"bearerAuth": []}]
    assert schema["components"]["securitySchemes"]["bearerAuth"]["scheme"] == "bearer"
    for endpoint in rest_server.catalog.values():
        assert "post" in schema["paths"][endpoint]
    properties = schema["components"]["schemas"]["Job"]["properties"]
    assert set(properties["state"]["enum"]) == {
        "queued", "running", "succeeded", "failed", "cancelled"}
    assert properties["exit_code"]["type"] == ["integer", "null"]
    assert {"get", "delete"}.issubset(schema["paths"]["/v1/jobs/{id}"])
