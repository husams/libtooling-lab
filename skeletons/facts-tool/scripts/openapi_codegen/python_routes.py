"""Emit paths and limits; runtime clients never need a YAML dependency."""

import json


def operations(spec: dict) -> dict[str, tuple[str, str]]:
    return {
        operation["operationId"]: (method.upper(), path.lstrip("/"))
        for path, path_item in spec["paths"].items()
        for method, operation in path_item.items()
        if method in {"get", "post", "put", "patch", "delete", "head", "options"}
    }


def routes(spec: dict) -> str:
    lines = [
        '"""Generated from src/apis/openapi/openapi.yaml; do not edit."""',
        "", "ROUTES: dict[str, tuple[str, str]] = {",
        *[
            f"    {json.dumps(name)}: ({json.dumps(method)}, {json.dumps(path)}),"
            for name, (method, path) in sorted(operations(spec).items())
        ],
        "}", "",
        f'MAX_ARGUMENTS = {spec["x-facts-limits"]["maxArguments"]}',
        f'MAX_ARGUMENT_BYTES = {spec["x-facts-limits"]["maxArgumentBytes"]}',
        "",
    ]
    return "\n".join(lines)
