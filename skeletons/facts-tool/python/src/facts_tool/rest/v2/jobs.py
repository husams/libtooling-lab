"""Operation-specific job handles and bounded polling."""

import time
from typing import TypeVar

import httpx

from ..domain_models import OperationError
from ..polling import remaining, validate_wait
from .job_state import JobFailed, JobMetadata, JobSnapshot, JobState, snapshot
from .wire import call, path

T = TypeVar("T")


class AnalysisJob[T]:
    def __init__(
        self, http: httpx.Client, family: str, model: type[T], value: JobSnapshot[T]
    ) -> None:
        self._http, self._family, self._model, self._value = http, family, model, value

    @property
    def metadata(self) -> JobMetadata:
        return self._value.metadata

    @property
    def id(self) -> str:
        return self._value.metadata.id

    @property
    def state(self) -> JobState:
        return self._value.metadata.state

    @property
    def result(self) -> T | None:
        return self._value.result

    @property
    def error(self) -> OperationError | None:
        return self._value.metadata.error

    @property
    def done(self) -> bool:
        return self.state in {"succeeded", "failed", "cancelled"}

    def refresh(self, *, budget: float | None = None) -> "AnalysisJob[T]":
        route = path(self._family + "/job", self.id)
        body = call(self._http, "GET", route, budget=budget)
        self._value = snapshot(self._model, body, self._family)
        return self

    def cancel(self) -> "AnalysisJob[T]":
        route = path(self._family + "/job", self.id)
        self._value = snapshot(
            self._model, call(self._http, "DELETE", route), self._family
        )
        return self

    def retry(self) -> "AnalysisJob[T]":
        """Retry a failed, cancelled, or partially failed job, preserving history."""
        body = call(
            self._http, "POST", path(self._family + "/job"), {"retry_of": self.id}
        )
        return AnalysisJob(
            self._http, self._family, self._model,
            snapshot(self._model, body, self._family),
        )

    def wait(self, *, timeout: float | None = None, poll_interval: float = 0.1) -> T:
        validate_wait(timeout, poll_interval)
        stop = None if timeout is None else time.monotonic() + timeout
        while not self.done or (self.state == "succeeded" and self.result is None):
            budget = remaining(stop, self.id, timeout)
            self.refresh(budget=budget)
            budget = remaining(stop, self.id, timeout)
            if not self.done:
                time.sleep(
                    poll_interval if budget is None else min(poll_interval, budget)
                )
        if self.state != "succeeded":
            raise JobFailed(self._value.metadata)
        assert self.result is not None
        return self.result
