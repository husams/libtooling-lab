"""Typed job creation and lazily paginated job/result collections."""

from typing import TypeVar

import httpx

from .analysis_models import Diagnostic
from .codec import decode
from .job_state import snapshot
from .jobs import AnalysisJob
from .pages import Collection, validate_limit
from .wire import call, path, query

T = TypeVar("T")
R = TypeVar("R")


class JobResource[T, R]:
    def __init__(
        self, http: httpx.Client, family: str, model: type[T], row: type[R]
    ) -> None:
        self._http, self._family, self._model, self._row = http, family, model, row

    def _job(
        self, body: dict[str, object], *, metadata_only: bool = False
    ) -> AnalysisJob[T]:
        return AnalysisJob(
            self._http,
            self._family,
            self._model,
            snapshot(self._model, body, self._family, metadata_only=metadata_only),
        )

    def _create(self, body: dict[str, object]) -> AnalysisJob[T]:
        route = path(self._family + "/job")
        return self._job(
            call(
                self._http,
                "POST",
                route,
                {k: v for k, v in body.items() if v is not None},
            )
        )

    def get(self, identifier: str) -> AnalysisJob[T]:
        return self._job(
            call(self._http, "GET", path(self._family + "/job", identifier))
        )

    def retry(self, identifier: str) -> AnalysisJob[T]:
        """Resubmit a retained failed/cancelled job using its original request."""
        return self._create({"retry_of": identifier})

    def list(self, *, limit: int = 50) -> Collection[AnalysisJob[T]]:
        route = path(self._family + "/job")
        if validate_limit(limit) > 500:
            raise ValueError("job limit cannot exceed 500")

        def fetch(cursor: str | None) -> dict[str, object]:
            return call(
                self._http, "GET", query(route, {"limit": limit, "cursor": cursor})
            )

        def parse(value: object) -> AnalysisJob[T]:
            from ..decoding import object_value

            return self._job(object_value(value), metadata_only=True)

        return Collection(fetch, parse)

    def cancel(self, identifier: str) -> AnalysisJob[T]:
        route = path(self._family + "/job", identifier)
        return self._job(call(self._http, "DELETE", route))

    def results(self, identifier: str, *, limit: int = 50) -> Collection[R]:
        return self._results(identifier, self._row, limit)

    def diagnostics(
        self, identifier: str, *, limit: int = 50
    ) -> Collection[Diagnostic]:
        return self._results(identifier, Diagnostic, limit, "diagnostics")

    def _results[V](
        self, identifier: str, row: type[V], limit: int, collection: str | None = None
    ) -> Collection[V]:
        route = path(self._family + "/job", identifier) + "/results"
        if validate_limit(limit) > 500:
            raise ValueError("job limit cannot exceed 500")

        def fetch(cursor: str | None) -> dict[str, object]:
            return call(
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

        return Collection(fetch, lambda value: decode(row, value))
