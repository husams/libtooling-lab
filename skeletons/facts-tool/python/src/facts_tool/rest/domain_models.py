"""Typed resource identifiers and structured asynchronous operation snapshots."""

from dataclasses import dataclass
from typing import Literal

from .errors import JobFailedError
from .models import JobState


@dataclass(frozen=True, slots=True)
class FileSelector:
    """A source path, optionally scoped to registered repository metadata."""

    path: str
    repo: str | None = None
    clone: str | None = None
    component: str | None = None


@dataclass(frozen=True, slots=True)
class Symbol:
    qualified_name: str
    kind: str
    usr: str
    file_id: int
    is_definition: bool
    path: str
    repo: str
    clone: str
    component: str


@dataclass(frozen=True, slots=True)
class SymbolPage:
    items: tuple[Symbol, ...]
    next_cursor: str | None


@dataclass(frozen=True, slots=True)
class IndexStatus:
    state: Literal["queued", "running", "ready", "failed"]
    pending: bool
    files: int
    symbols: int
    error: str | None
    updated_at: int | None


@dataclass(frozen=True, slots=True)
class OperationError:
    code: str
    message: str
    details: dict[str, object] | None = None


@dataclass(frozen=True, slots=True)
class DomainJob:
    """Job snapshot; fetch get_job for result data omitted from job listings."""

    id: str
    operation: Literal["extract", "match", "dependencies"]
    state: JobState
    created_at: int
    result: dict[str, object] | None
    error: OperationError | None
    started_at: int | None = None
    finished_at: int | None = None

    @property
    def done(self) -> bool:
        return self.state in {"succeeded", "failed", "cancelled"}

    @property
    def succeeded(self) -> bool:
        return self.state == "succeeded"

    def raise_for_status(self) -> "DomainJob":
        """Raise for completed failures without polling or cancelling."""
        if self.done and not self.succeeded:
            raise JobFailedError(self)
        return self
