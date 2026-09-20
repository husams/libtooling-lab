"""Global index status and explicitly requested rebuild jobs."""

from dataclasses import dataclass

import httpx

from ..domain_models import IndexStatus as LegacyIndexStatus
from .codec import decode
from .job_resource import JobResource
from .jobs import AnalysisJob
from .wire import call, path


@dataclass(frozen=True, slots=True)
class IndexStatus(LegacyIndexStatus):
    index_revision: str | None


@dataclass(frozen=True, slots=True)
class IndexResult:
    index_revision: str


class Index(JobResource[IndexResult, IndexResult]):
    def __init__(self, http: httpx.Client) -> None:
        super().__init__(http, "index", IndexResult, IndexResult)

    def status(self) -> IndexStatus:
        return decode(IndexStatus, call(self._http, "GET", path("index")))

    def create(self) -> AnalysisJob[IndexResult]:
        return self._create({})
