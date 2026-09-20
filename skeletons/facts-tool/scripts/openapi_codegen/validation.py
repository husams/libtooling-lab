"""Validate the specification and the supported native routing contract."""
import re

from openapi_spec_validator import validate_spec

from .requests import validate_requests

METHODS = {"get", "post", "delete", "put", "patch", "head", "options", "trace"}
LIMITS = ("maxBodyBytes", "maxHeaderBytes", "maxArguments", "maxArgumentBytes")


def operations(document: dict):
    for path, item in document["paths"].items():
        for method, operation in item.items():
            if method in METHODS:
                yield path, method, operation


def validate(document: dict) -> None:
    if document.get("openapi") != "3.1.0":
        raise ValueError("The generator requires OpenAPI 3.1.0")
    validate_spec(document)
    validate_requests(document)
    limits = document.get("x-facts-limits", {})
    for name in LIMITS:
        ceiling = (1 << (64 if name == "maxBodyBytes" else 32)) - 1
        if type(limits.get(name)) is not int or not 0 < limits[name] <= ceiling:
            raise ValueError(f"x-facts-limits.{name} must be a positive integer <= {ceiling}")
    schemas = document["components"]["schemas"]
    bounds = [(schemas["Argument"], "maxLength", "maxArgumentBytes"),
              (schemas["Argument"], "x-max-utf8-bytes", "maxArgumentBytes"),
              (schemas["Arguments"], "x-max-combined-arguments", "maxArguments")]
    bounds += [(schemas[name]["properties"]["arguments"], "maxItems", "maxArguments")
               for name in ("Arguments", "JobRequest")]
    for schema, keyword, limit in bounds:
        if schema.get(keyword) != limits[limit]:
            raise ValueError(f"Schema {keyword} differs from x-facts-limits.{limit}")
    identifiers = set()
    for path, _method, operation in operations(document):
        identifier = operation.get("operationId", "")
        if not re.fullmatch(r"[a-zA-Z][a-zA-Z0-9_]*", identifier):
            raise ValueError(f"Invalid generated operationId: {identifier!r}")
        if identifier in identifiers:
            raise ValueError(f"Duplicate operationId: {identifier}")
        identifiers.add(identifier)
        parameters = re.findall(r"\{([^}]+)\}", path)
        expected = {"command": ["commandPath"], "getJob": ["id"],
                    "cancelJob": ["id"]}.get(identifier, [])
        if parameters != expected or (parameters and not path.endswith(
                "/{" + parameters[0] + "}")):
            raise ValueError(f"Unsupported path parameters for {identifier}: {path}")
