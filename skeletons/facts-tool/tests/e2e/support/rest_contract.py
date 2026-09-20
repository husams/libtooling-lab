"""Validate live JSON payloads using the published OpenAPI response definitions."""
from jsonschema import Draft202012Validator


def validate_response(document, method, path, status, body):
    responses = document["paths"][path][method.lower()]["responses"]
    response = responses.get(str(status), responses.get("default"))
    assert response, f"Undocumented response: {method} {path} {status}"
    while "$ref" in response:
        parts = response["$ref"].removeprefix("#/").split("/")
        response = document
        for part in parts:
            response = response[part.replace("~1", "/").replace("~0", "~")]
    schema = response["content"]["application/json"]["schema"]
    Draft202012Validator({"components": document["components"], **schema}).validate(body)
