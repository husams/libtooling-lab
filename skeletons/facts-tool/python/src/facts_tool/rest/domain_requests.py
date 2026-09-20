"""Validate resource input and encode symbol filters without exposing DB paths."""

from dataclasses import asdict
from urllib.parse import urlencode

from .domain_models import FileSelector
from .endpoints import endpoint


def text(value: object, name: str, maximum: int = 4096) -> str:
    if not isinstance(value, str) or not value or "\0" in value:
        raise ValueError(f"{name} must be a nonempty string without NUL")
    if len(value.encode("utf-8")) > maximum:
        raise ValueError(f"{name} exceeds {maximum} UTF-8 bytes")
    return value


def file_request(file: FileSelector, query: str | None = None) -> dict[str, object]:
    if not isinstance(file, FileSelector):
        raise TypeError("file must be a FileSelector")
    selector = {key: text(value, key) for key, value in asdict(file).items()
                if value is not None}
    if "path" not in selector:
        raise ValueError("path is required")
    body: dict[str, object] = {"file": selector}
    if query is not None:
        body["query"] = text(query, "query", 262144)
    return body


def symbol_route(
    qualified_name: str, kind: str | None, usr: str | None, repo: str | None,
    component: str | None, limit: int, cursor: str | None,
) -> tuple[str, str]:
    if type(limit) is not int or not 1 <= limit <= 500:
        raise ValueError("limit must be an integer from 1 through 500")
    filters: dict[str, str | int] = {
        "qualified_name": text(qualified_name, "qualified_name"), "limit": limit,
    }
    for key, value in (("kind", kind), ("usr", usr), ("repo", repo),
                       ("component", component), ("cursor", cursor)):
        if value is not None:
            filters[key] = text(value, key, 256 if key == "cursor" else 4096)
    method, path = endpoint("findSymbols")
    return method, path + "?" + urlencode(filters)
