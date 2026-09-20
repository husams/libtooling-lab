"""V2 resources with shared connection lifecycle and explicit v1 compatibility."""

import httpx

from ..async_client import AsyncClient as LegacyClient
from .async_callgraphs import CallGraphs
from .async_compatibility import CompatibleDependencies
from .async_discovery import Imports, Scans
from .async_file_analyses import Extractions, Matches
from .async_files import Components, Directories, Files
from .async_index import Index
from .async_repositories import Repositories
from .async_server import Server
from .async_symbols import Symbols
from .async_variable_flow import VariableFlow
from .async_watcher import Watcher


class AsyncClient(LegacyClient):
    dependencies: CompatibleDependencies

    def __init__(
        self,
        base_url: str,
        *,
        token: str | None = None,
        timeout: float = 10.0,
        transport: httpx.AsyncBaseTransport | None = None,
    ) -> None:
        super().__init__(base_url, token=token, timeout=timeout, transport=transport)
        self.legacy = LegacyClient.__new__(LegacyClient)
        self.legacy._http = self._http
        self.repositories = Repositories(self._http)
        self.components = Components(self._http)
        self.directories = Directories(self._http)
        self.files = Files(self._http)
        self.symbols = Symbols(self._http)
        self.extractions = Extractions(self._http)
        self.matches = Matches(self._http)
        self.dependencies = CompatibleDependencies(self._http, self.legacy)
        self.callgraphs = CallGraphs(self._http)
        self.variable_flow = VariableFlow(self._http)
        self.imports = Imports(self._http)
        self.scans = Scans(self._http)
        self.index = Index(self._http)
        self.watcher = Watcher(self._http)
        self.server = Server(self._http)
