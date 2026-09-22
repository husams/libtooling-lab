"""Typed job creation and lazily paginated job/result collections."""

from typing import TypeVar

import httpx

from .analysis_models import Diagnostic, FileFailure
from .async_jobs import AsyncAnalysisJob
from .codec import decode
from .job_state import snapshot
from .pages import AsyncCollection, validate_limit
from .wire import async_call, path, query

T = TypeVar("T")
R = TypeVar("R")


class JobResource[T, R]:
    def __init__(
        self, http: httpx.AsyncClient, family: str, model: type[T], row: type[R]
    ) -> None:
        self._http, self._family, self._model, self._row = http, family, model, row

    def _job(
        self, body: dict[str, object], *, metadata_only: bool = False
    ) -> AsyncAnalysisJob[T]:
        return AsyncAnalysisJob(
            self._http,
            self._family,
            self._model,
            snapshot(self._model, body, self._family, metadata_only=metadata_only),
        )

    async def _create(self, body: dict[str, object]) -> AsyncAnalysisJob[T]:
        route = path(self._family + "/job")
        return self._job(
            await async_call(
                self._http,
                "POST",
                route,
                {k: v for k, v in body.items() if v is not None},
            )
        )

    async def get(self, identifier: str) -> AsyncAnalysisJob[T]:
        return self._job(
            await async_call(self._http, "GET", path(self._family + "/job", identifier))
        )

    async def retry(self, identifier: str) -> AsyncAnalysisJob[T]:
        """Retry a retained failed, cancelled, or partially failed job."""
        return await self._create({"retry_of": identifier})

    def list(self, *, limit: int = 50) -> AsyncCollection[AsyncAnalysisJob[T]]:
        route = path(self._family + "/job")
        if validate_limit(limit) > 500:
            raise ValueError("job limit cannot exceed 500")

        async def fetch(cursor: str | None) -> dict[str, object]:
            return await async_call(
                self._http, "GET", query(route, {"limit": limit, "cursor": cursor})
            )

        def parse(value: object) -> AsyncAnalysisJob[T]:
            from ..decoding import object_value

            return self._job(object_value(value), metadata_only=True)

        return AsyncCollection(fetch, parse)

    async def cancel(self, identifier: str) -> AsyncAnalysisJob[T]:
        route = path(self._family + "/job", identifier)
        return self._job(await async_call(self._http, "DELETE", route))

    def results(self, identifier: str, *, limit: int = 50) -> AsyncCollection[R]:
        return self._results(identifier, self._row, limit)

    def failed_files(
        self, identifier: str, *, limit: int = 50
    ) -> AsyncCollection[FileFailure]:
        """Read retained per-file errors for extraction, matching, or dependencies."""
        return self._results(identifier, FileFailure, limit, "failed_files")

    def diagnostics(
        self, identifier: str, *, limit: int = 50
    ) -> AsyncCollection[Diagnostic]:
        return self._results(identifier, Diagnostic, limit, "diagnostics")

    def _results[V](
        self, identifier: str, row: type[V], limit: int, collection: str | None = None
    ) -> AsyncCollection[V]:
        route = path(self._family + "/job", identifier) + "/results"
        if validate_limit(limit) > 500:
            raise ValueError("job limit cannot exceed 500")

        async def fetch(cursor: str | None) -> dict[str, object]:
            return await async_call(
                self._http,
                "GET",
                query(
                    route,
                    {
                        "limit": limit,
                        "cursor": cursor,
                        "collection": collection,
                    },
                ),
            )

        return AsyncCollection(fetch, lambda value: decode(row, value))
