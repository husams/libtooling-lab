"""Manual import and scan jobs complement automatic repository monitoring."""

import httpx

from .discovery_models import (
    DiscoveredDatabase,
    ImportedDatabase,
    ImportResult,
    ScanResult,
    ScanWarning,
)
from .job_resource import JobResource
from .jobs import AnalysisJob
from .pages import Collection
from .selections import ScanSelection


class Imports(JobResource[ImportResult, ImportedDatabase]):
    def __init__(self, http: httpx.Client) -> None:
        super().__init__(http, "import", ImportResult, ImportedDatabase)

    def create(
        self,
        *,
        repository: str | None = None,
        compilation_database: str | None = None,
        selection: ScanSelection | None = None,
    ) -> AnalysisJob[ImportResult]:
        return self._create(
            {
                "repository": repository,
                "compilation_database": compilation_database,
                "selection": selection,
            }
        )


class Scans(JobResource[ScanResult, ScanWarning]):
    def __init__(self, http: httpx.Client) -> None:
        super().__init__(http, "scan", ScanResult, ScanWarning)

    def create(self, *, selection: ScanSelection) -> AnalysisJob[ScanResult]:
        return self._create({"selection": selection})

    def warnings(self, identifier: str, *, limit: int = 50) -> Collection[ScanWarning]:
        return self.results(identifier, limit=limit)

    def databases(
        self, identifier: str, *, limit: int = 50
    ) -> Collection[DiscoveredDatabase]:
        return self._results(identifier, DiscoveredDatabase, limit, "databases")
