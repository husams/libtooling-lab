"""Reject request-contract drift beyond the implemented typed resource inputs."""

from .requests import shape

BODY_SCHEMAS = {
    "extract": "ExtractionRequest", "match": "MatchRequest", "dependencies": "FileRequest",
}


def validate_domain_requests(document: dict) -> None:
    schemas = document["components"]["schemas"]
    names = {
        "FileSelector": ({"path", "repo", "clone", "component"}, ["path"]),
        "FileRequest": ({"file"}, ["file"]),
        "ExtractionRequest": ({"file", "force"}, ["file"]),
        "MatchRequest": ({"file", "query", "traversal", "relation_kind", "capture_source"},
                         ["file", "query"]),
    }
    for name, (properties, required) in names.items():
        schema = schemas[name]
        shape(schema, {"type": "object", "required": required,
                       "additionalProperties": False}, {"properties"}, name)
        if set(schema["properties"]) != properties:
            raise ValueError(f"Unsupported {name} request properties")
    selector = {"$ref": "#/components/schemas/FileSelector"}
    for name in BODY_SCHEMAS.values():
        if schemas[name]["properties"]["file"] != selector:
            raise ValueError(f"Unsupported {name}.file selector")
    for path_item in document["paths"].values():
        for operation in path_item.values():
            if not isinstance(operation, dict):
                continue
            name = BODY_SCHEMAS.get(operation.get("operationId"))
            if name:
                body = operation.get("requestBody", {})
                content = body.get("content", {})
                expected = {"$ref": f"#/components/schemas/{name}"}
                if body.get("required") is not True or set(content) != {"application/json"}:
                    raise ValueError(f"{name} requires an application/json request body")
                if content["application/json"].get("schema") != expected:
                    raise ValueError(f"Unsupported {name} request body schema")
