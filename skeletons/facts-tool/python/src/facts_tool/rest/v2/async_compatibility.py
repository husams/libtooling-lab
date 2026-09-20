"""Callable compatibility for the former flat dependencies(file) method."""

import httpx

from ..async_client import AsyncClient as LegacyClient
from ..domain_models import DomainJob, FileSelector
from .async_file_analyses import Dependencies


class CompatibleDependencies(Dependencies):
    def __init__(self, http: httpx.AsyncClient, legacy: LegacyClient) -> None:
        super().__init__(http)
        self._legacy = legacy

    async def __call__(self, file: FileSelector) -> DomainJob:
        return await self._legacy.dependencies(file)
