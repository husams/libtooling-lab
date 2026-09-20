"""Immutable snapshots of server-local jobs."""

from dataclasses import dataclass
from typing import Literal

from .errors import JobFailedError

JobState = Literal["queued", "running", "succeeded", "failed", "cancelled"]


@dataclass(frozen=True, slots=True)
class Job:
    """Output is absent in lists; timestamps are Unix epoch milliseconds.

    Output on unfinished jobs is not final. ``truncated`` means the captured
    streams are incomplete, even when the command otherwise succeeded.
    """

    id: str
    state: JobState
    arguments: tuple[str, ...]
    exit_code: int | None
    stdout: str | None
    stderr: str | None
    truncated: bool
    timed_out: bool
    created_at: int
    started_at: int | None = None
    finished_at: int | None = None

    @property
    def done(self) -> bool:
        return self.state in {"succeeded", "failed", "cancelled"}

    @property
    def succeeded(self) -> bool:
        return self.state == "succeeded" and self.exit_code == 0 and not self.timed_out

    def raise_for_status(self) -> "Job":
        """Raise only for completed failures; this never polls or cancels."""
        if self.done and not self.succeeded:
            raise JobFailedError(self)
        return self
