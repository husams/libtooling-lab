"""Validate structured resource responses at the HTTP boundary."""

from typing import Literal, cast

from .decoding import field, object_list, object_value
from .domain_models import DomainJob, IndexStatus, OperationError, Symbol, SymbolPage
from .errors import ProtocolError
from .models import JobState

IndexStatusState = Literal["queued", "running", "ready", "failed"]
Operation = Literal["extract", "match", "dependencies"]


def string(body: dict[str, object], key: str) -> str:
    return cast(str, field(body, key, str))


def integer(body: dict[str, object], key: str) -> int:
    return cast(int, field(body, key, int))


def optional(body: dict[str, object], key: str, kind: type) -> object:
    if key not in body:
        raise ProtocolError(f"Missing resource field: {key}")
    return field(body, key, kind, optional=True)


def symbol_page(body: dict[str, object]) -> SymbolPage:
    items = tuple(Symbol(
        qualified_name=string(row, "qualified_name"), kind=string(row, "kind"),
        usr=string(row, "usr"), file_id=integer(row, "file_id"),
        is_definition=cast(bool, field(row, "is_definition", bool)),
        path=string(row, "path"), repo=string(row, "repo"),
        clone=string(row, "clone"), component=string(row, "component"),
    ) for row in object_list(body, "items"))
    return SymbolPage(items, cast(str | None, optional(body, "next_cursor", str)))


def index_status(body: dict[str, object]) -> IndexStatus:
    state = string(body, "state")
    if state not in {"queued", "running", "ready", "failed"}:
        raise ProtocolError("Invalid index state")
    files, symbols = integer(body, "files"), integer(body, "symbols")
    if min(files, symbols) < 0:
        raise ProtocolError("Index counts cannot be negative")
    return IndexStatus(
        state=cast("IndexStatusState", state),
        pending=cast(bool, field(body, "pending", bool)), files=files, symbols=symbols,
        error=cast(str | None, optional(body, "error", str)),
        updated_at=cast(int | None, optional(body, "updated_at", int)),
    )


def domain_job(body: dict[str, object]) -> DomainJob:
    state, operation = string(body, "state"), string(body, "operation")
    job_id = string(body, "id")
    states = {"queued", "running", "succeeded", "failed", "cancelled"}
    if not job_id or state not in states:
        raise ProtocolError("Invalid domain job id or state")
    if operation not in {"extract", "match", "dependencies"}:
        raise ProtocolError("Invalid domain operation")
    error_value = optional(body, "error", dict)
    error = None
    if error_value is not None:
        error_body = object_value(error_value)
        details = error_body.get("details")
        error = OperationError(
            string(error_body, "code"), string(error_body, "message"),
            None if details is None else object_value(details))
    result = optional(body, "result", dict)
    return DomainJob(
        id=job_id, operation=cast("Operation", operation), state=cast(JobState, state),
        created_at=integer(body, "created_at"),
        result=None if result is None else object_value(result), error=error,
        started_at=cast(int | None, field(body, "started_at", int, optional=True)),
        finished_at=cast(int | None, field(body, "finished_at", int, optional=True)),
    )
