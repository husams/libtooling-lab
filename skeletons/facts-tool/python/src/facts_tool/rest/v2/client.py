"""V2 resources with shared connection lifecycle and explicit v1 compatibility."""

import httpx

from ..client import Client as LegacyClient
from .callgraphs import CallGraphs
from .compatibility import CompatibleDependencies
from .discovery import Imports, Scans
from .file_analyses import Extractions, Matches
from .files import Components, Directories, Files
from .index import Index
from .repositories import Repositories
from .server import Server
from .symbols import Symbols
from .variable_flow import VariableFlow
from .watcher import Watcher


class Client(LegacyClient):
    dependencies: CompatibleDependencies

    def __init__(
        self,
        base_url: str,
        *,
        token: str | None = None,
        timeout: float = 10.0,
        transport: httpx.BaseTransport | None = None,
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
