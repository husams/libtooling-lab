"""Callable compatibility for the former flat dependencies(file) method."""

import httpx

from ..client import Client as LegacyClient
from ..domain_models import DomainJob, FileSelector
from .file_analyses import Dependencies


class CompatibleDependencies(Dependencies):
    def __init__(self, http: httpx.Client, legacy: LegacyClient) -> None:
        super().__init__(http)
        self._legacy = legacy

    def __call__(self, file: FileSelector) -> DomainJob:
        return self._legacy.dependencies(file)
