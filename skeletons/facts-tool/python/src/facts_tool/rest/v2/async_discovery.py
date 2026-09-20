"""Manual import and scan jobs complement automatic repository monitoring."""

import httpx

from .async_job_resource import JobResource
from .async_jobs import AsyncAnalysisJob
from .discovery_models import (
    DiscoveredDatabase,
    ImportedDatabase,
    ImportResult,
    ScanResult,
    ScanWarning,
)
from .pages import AsyncCollection
from .selections import ScanSelection


class Imports(JobResource[ImportResult, ImportedDatabase]):
    def __init__(self, http: httpx.AsyncClient) -> None:
        super().__init__(http, "import", ImportResult, ImportedDatabase)

    async def create(
        self,
        *,
        repository: str | None = None,
        compilation_database: str | None = None,
        selection: ScanSelection | None = None,
    ) -> AsyncAnalysisJob[ImportResult]:
        return await self._create(
            {
                "repository": repository,
                "compilation_database": compilation_database,
                "selection": selection,
            }
        )


class Scans(JobResource[ScanResult, ScanWarning]):
    def __init__(self, http: httpx.AsyncClient) -> None:
        super().__init__(http, "scan", ScanResult, ScanWarning)

    async def create(self, *, selection: ScanSelection) -> AsyncAnalysisJob[ScanResult]:
        return await self._create({"selection": selection})

    def warnings(
        self, identifier: str, *, limit: int = 50
    ) -> AsyncCollection[ScanWarning]:
        return self.results(identifier, limit=limit)

    def databases(
        self, identifier: str, *, limit: int = 50
    ) -> AsyncCollection[DiscoveredDatabase]:
        return self._results(identifier, DiscoveredDatabase, limit, "databases")
