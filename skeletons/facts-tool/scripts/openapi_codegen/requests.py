"""Keep the generated contract within implemented request validation semantics."""
ANNOTATIONS = {"title", "description", "examples", "example", "deprecated", "default", "$comment"}


def shape(schema: dict, expected: dict, variable: set[str], name: str) -> None:
    for keyword, value in expected.items():
        if schema.get(keyword) != value:
            raise ValueError(f"Unsupported {name}.{keyword}; expected {value!r}")
    unknown = set(schema) - set(expected) - variable - ANNOTATIONS
    unknown = {key for key in unknown if not key.startswith("x-")}
    if unknown:
        raise ValueError(f"Unsupported request constraints in {name}: {sorted(unknown)}")


def validate_requests(document: dict) -> None:
    schemas = document["components"]["schemas"]
    shape(schemas["Argument"], {"type": "string", "pattern": r"^[^\u0000]*$"},
          {"maxLength"}, "Argument")
    for name, minimum in (("Arguments", 0), ("JobRequest", 1)):
        schema = schemas[name]
        shape(schema, {"type": "object", "required": ["arguments"],
                       "additionalProperties": False}, {"properties"}, name)
        if set(schema["properties"]) != {"arguments"}:
            raise ValueError(f"Unsupported {name} request properties")
        shape(schema["properties"]["arguments"], {
            "type": "array", "minItems": minimum,
            "items": {"$ref": "#/components/schemas/Argument"},
        }, {"maxItems"}, f"{name}.arguments")
    for path_item in document["paths"].values():
        for operation in path_item.values():
            if not isinstance(operation, dict):
                continue
            name = {"command": "Arguments", "submit": "JobRequest"}.get(
                operation.get("operationId"))
            if name is None:
                continue
            body = operation.get("requestBody", {})
            content = body.get("content", {})
            expected = {"$ref": f"#/components/schemas/{name}"}
            if body.get("required") is not True or set(content) != {"application/json"}:
                raise ValueError(f"{name} requires an application/json request body")
            if content["application/json"].get("schema") != expected:
                raise ValueError(f"Unsupported {name} request body schema")
