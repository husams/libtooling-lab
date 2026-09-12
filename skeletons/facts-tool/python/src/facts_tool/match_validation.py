from collections.abc import Mapping
from pathlib import Path
from typing import Any

from .errors import fail


def as_mapping(value: Any, field: str) -> Mapping[str, Any]:
    if not isinstance(value, Mapping):
        fail("E_SCHEMA", f"{field} must be an object")
    return value


def required(value: Mapping[str, Any], field: str) -> Any:
    if field not in value:
        fail("E_SCHEMA", f"missing required field {field}")
    return value[field]


def string(value: Any, field: str, absolute: bool = False) -> str:
    if not isinstance(value, str) or not value:
        fail("E_SCHEMA", f"{field} must be a non-empty string")
    if absolute and not Path(value).is_absolute():
        fail("E_SCHEMA", f"{field} must be an absolute path")
    return value


def nullable_string(value: Any, field: str) -> str | None:
    if value is None:
        return None
    return string(value, field)


def boolean(value: Any, field: str) -> bool:
    if type(value) is not bool:
        fail("E_SCHEMA", f"{field} must be a boolean")
    return value


def integer(value: Any, field: str, minimum: int = 0) -> int:
    if type(value) is not int or value < minimum:
        fail("E_SCHEMA", f"{field} must be an integer >= {minimum}")
    return value


def array(value: Any, field: str) -> list[Any]:
    if not isinstance(value, list):
        fail("E_SCHEMA", f"{field} must be an array")
    return value
