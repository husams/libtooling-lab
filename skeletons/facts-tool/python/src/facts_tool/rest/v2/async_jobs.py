"""Operation-specific job handles and bounded polling."""

import asyncio
import time
from typing import TypeVar

import httpx

from ..domain_models import OperationError
from ..errors import JobTimeoutError
from ..polling import remaining, validate_wait
from .job_state import JobFailed, JobMetadata, JobSnapshot, JobState, snapshot
from .wire import async_call, path

T = TypeVar("T")


class AsyncAnalysisJob[T]:
    def __init__(
        self,
        http: httpx.AsyncClient,
        family: str,
        model: type[T],
        value: JobSnapshot[T],
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

    async def refresh(self, *, budget: float | None = None) -> "AsyncAnalysisJob[T]":
        route = path(self._family + "/job", self.id)
        body = await async_call(self._http, "GET", route, budget=budget)
        self._value = snapshot(self._model, body, self._family)
        return self

    async def cancel(self) -> "AsyncAnalysisJob[T]":
        route = path(self._family + "/job", self.id)
        self._value = snapshot(
            self._model, await async_call(self._http, "DELETE", route), self._family
        )
        return self

    async def wait(
        self, *, timeout: float | None = None, poll_interval: float = 0.1
    ) -> T:
        validate_wait(timeout, poll_interval)
        stop = None if timeout is None else time.monotonic() + timeout
        while not self.done or (self.state == "succeeded" and self.result is None):
            budget = remaining(stop, self.id, timeout)
            try:
                async with asyncio.timeout(budget):
                    await self.refresh(budget=budget)
            except TimeoutError as error:
                raise JobTimeoutError(self.id, timeout) from error
            budget = remaining(stop, self.id, timeout)
            if not self.done:
                await asyncio.sleep(
                    poll_interval if budget is None else min(poll_interval, budget)
                )
        if self.state != "succeeded":
            raise JobFailed(self._value.metadata)
        assert self.result is not None
        return self.result
