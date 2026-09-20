"""Global index status and explicitly requested rebuild jobs."""

import httpx

from .async_job_resource import JobResource
from .async_jobs import AsyncAnalysisJob
from .codec import decode
from .index import IndexResult, IndexStatus
from .wire import async_call, path


class Index(JobResource[IndexResult, IndexResult]):
    def __init__(self, http: httpx.AsyncClient) -> None:
        super().__init__(http, "index", IndexResult, IndexResult)

    async def status(self) -> IndexStatus:
        return decode(IndexStatus, await async_call(self._http, "GET", path("index")))

    async def create(self) -> AsyncAnalysisJob[IndexResult]:
        return await self._create({})
