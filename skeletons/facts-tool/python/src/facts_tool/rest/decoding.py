"""Strict decoding keeps malformed remote data outside the typed job model."""

from typing import cast

from .errors import ProtocolError
from .models import Job, JobState


def object_value(value: object) -> dict[str, object]:
    if not isinstance(value, dict) or any(not isinstance(key, str) for key in value):
        raise ProtocolError("Expected a JSON object")
    return cast(dict[str, object], value)


def object_list(body: dict[str, object], key: str) -> list[dict[str, object]]:
    values = body.get(key)
    if not isinstance(values, list):
        raise ProtocolError(f"Expected an array in {key}")
    return [object_value(value) for value in values]


def field(
    body: dict[str, object], key: str, kind: type, *, optional: bool = False,
) -> object:
    value = body.get(key)
    if optional and value is None:
        return None
    if type(value) is not kind:
        raise ProtocolError(f"Invalid or missing job field: {key}")
    return value


def job(value: object, *, metadata: bool = False) -> Job:
    body = object_value(value)
    job_id = cast(str, field(body, "id", str))
    state = field(body, "state", str)
    states = {"queued", "running", "succeeded", "failed", "cancelled"}
    if not job_id or state not in states:
        raise ProtocolError("Invalid job id or state")
    values = field(body, "arguments", list)
    if not isinstance(values, list) or not all(isinstance(arg, str) for arg in values):
        raise ProtocolError("Job arguments must be an array of strings")
    if "exit_code" not in body:
        raise ProtocolError("Missing job field: exit_code")
    return Job(
        id=job_id,
        state=cast(JobState, state),
        arguments=tuple(values),
        exit_code=cast(int | None, field(body, "exit_code", int, optional=True)),
        stdout=cast(str | None, field(body, "stdout", str, optional=metadata)),
        stderr=cast(str | None, field(body, "stderr", str, optional=metadata)),
        truncated=cast(bool, field(body, "truncated", bool)),
        timed_out=cast(bool, field(body, "timed_out", bool)),
        created_at=cast(int, field(body, "created_at", int)),
        started_at=cast(int | None, field(body, "started_at", int, optional=True)),
        finished_at=cast(int | None, field(body, "finished_at", int, optional=True)),
        working_directory=cast(str | None, field(body, "working_directory", str, optional=True)),
    )


def command_catalog(body: dict[str, object]) -> list[dict[str, object]]:
    values = object_list(body, "commands")
    if any(
        not isinstance(item.get(key), str)
        for item in values for key in ("path", "endpoint")
    ):
        raise ProtocolError("Invalid command catalog")
    return values
