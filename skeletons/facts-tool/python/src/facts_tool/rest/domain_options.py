"""Typed analysis controls and request-body validation."""

from typing import Literal

from .domain_models import FileSelector
from .domain_requests import file_request, text

Traversal = Literal["AsIs", "IgnoreUnlessSpelledInSource"]


def boolean(value: object, name: str) -> bool:
    if type(value) is not bool:
        raise TypeError(f"{name} must be a bool")
    return value


def extraction_request(file: FileSelector, force: bool) -> dict[str, object]:
    return file_request(file) | {"force": boolean(force, "force")}


def match_request(
    file: FileSelector, query: str, traversal: Traversal,
    relation_kind: str | None, capture_source: bool,
) -> dict[str, object]:
    if traversal not in {"AsIs", "IgnoreUnlessSpelledInSource"}:
        raise ValueError("Invalid matcher traversal")
    result = file_request(file, text(query, "query", 262144)) | {
        "traversal": traversal,
        "capture_source": boolean(capture_source, "capture_source"),
    }
    if relation_kind is not None:
        result["relation_kind"] = text(relation_kind, "relation_kind")
    return result
