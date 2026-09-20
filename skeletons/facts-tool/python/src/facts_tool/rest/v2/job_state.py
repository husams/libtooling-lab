"""Typed job metadata; operation outputs are decoded separately."""

from dataclasses import dataclass
from typing import Literal, TypeVar

from ..domain_models import OperationError
from ..errors import ProtocolError
from .codec import decode

T = TypeVar("T")
JobState = Literal[
    "queued", "running", "cancelling", "succeeded", "failed", "cancelled"
]


@dataclass(frozen=True, slots=True)
class JobMetadata:
    id: str
    operation: str
    state: JobState
    created_at: int
    started_at: int | None = None
    finished_at: int | None = None
    error: OperationError | None = None


@dataclass(frozen=True, slots=True)
class JobSnapshot[T]:
    metadata: JobMetadata
    result: T | None


class JobFailed(RuntimeError):
    def __init__(self, metadata: JobMetadata) -> None:
        self.job_id, self.state, self.error = (
            metadata.id,
            metadata.state,
            metadata.error,
        )
        detail = "" if metadata.error is None else f": {metadata.error.message}"
        super().__init__(f"Job {metadata.id} {metadata.state}{detail}")


def snapshot[T](
    model: type[T],
    body: dict[str, object],
    operation: str,
    *,
    metadata_only: bool = False,
) -> JobSnapshot[T]:
    metadata = decode(JobMetadata, body)
    if metadata.operation != operation:
        raise ProtocolError("Server returned the wrong job operation")
    result = body.get("result")
    if metadata.state == "succeeded" and result is None and not metadata_only:
        raise ProtocolError("Succeeded job has no result")
    return JobSnapshot(metadata, None if result is None else decode(model, result))
