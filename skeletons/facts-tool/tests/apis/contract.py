"""Validate actual response objects against the server's published schema."""
from jsonschema import Draft202012Validator


def response_matches(document, method, path, status, body):
    responses = document["paths"][path][method.lower()]["responses"]
    response = responses.get(str(status), responses.get("default"))
    assert response, f"Undocumented response: {method} {path} {status}"
    while "$ref" in response:
        target = response["$ref"].removeprefix("#/").split("/")
        response = document
        for part in target:
            response = response[part.replace("~1", "/").replace("~0", "~")]
    schema = response["content"]["application/json"]["schema"]
    Draft202012Validator({"components": document["components"], **schema}).validate(body)


def documented_request(server, document, method, path, body=None, schema_path=None, **options):
    status, payload = server.api.request(method, path, body, **options)
    response_matches(document, method, schema_path or path, status, payload)
    return status, payload
