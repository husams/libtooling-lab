"""Validate argument arrays and endpoint identifiers, never shell commands."""

import re

from .generated.routes import MAX_ARGUMENT_BYTES, MAX_ARGUMENTS

_SEGMENT = re.compile(r"[A-Za-z0-9_-]+")


def identifier(value: str) -> str:
    if not isinstance(value, str) or not _SEGMENT.fullmatch(value):
        raise ValueError("job_id must be a nonempty URL-safe identifier")
    return value


def command_path(value: str) -> str:
    if not isinstance(value, str) or not all(
        _SEGMENT.fullmatch(part) for part in value.split("/")
    ):
        raise ValueError("command path must contain slash-separated command names")
    return value


def arguments(values: tuple[str, ...], *, prefix_count: int = 0) -> dict[str, object]:
    if not 1 <= len(values) + prefix_count <= MAX_ARGUMENTS:
        raise ValueError(f"Expected between 1 and {MAX_ARGUMENTS} CLI arguments")
    for value in values:
        if not isinstance(value, str):
            raise ValueError("CLI arguments must be strings")
        try:
            valid = (
                "\0" not in value and len(value.encode("utf-8")) <= MAX_ARGUMENT_BYTES
            )
        except UnicodeEncodeError:
            valid = False
        if not valid:
            raise ValueError("Invalid CLI argument length, Unicode, or embedded NUL")
    return {"arguments": list(values)}
