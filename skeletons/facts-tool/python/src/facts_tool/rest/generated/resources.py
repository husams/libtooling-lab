"""Generated from the OpenAPI resource operations; do not edit."""

import httpx

from ..domain_decoding import domain_job, index_status, symbol_page
from ..domain_models import DomainJob, FileSelector, IndexStatus, SymbolPage
from ..domain_options import Traversal, extraction_request, match_request
from ..domain_requests import file_request, symbol_route
from ..endpoints import endpoint
from ..transport import request


class ResourceOperations:
    _http: httpx.Client

    def find_symbols(
        self, qualified_name: str, *, kind: str | None = None,
        usr: str | None = None, repo: str | None = None,
        component: str | None = None, limit: int = 50, cursor: str | None = None,
    ) -> SymbolPage:
        route = symbol_route(qualified_name, kind, usr, repo, component, limit, cursor)
        return symbol_page(request(self._http, *route))

    def index_status(self) -> IndexStatus:
        return index_status(request(self._http, *endpoint("indexStatus")))

    def extract(self, file: FileSelector, *, force: bool = False) -> DomainJob:
        body = extraction_request(file, force)
        route = endpoint("extract")
        return domain_job(request(self._http, *route, body=body))

    def match(
        self, file: FileSelector, query: str, *,
        traversal: Traversal = "AsIs", relation_kind: str | None = None,
        capture_source: bool = False,
    ) -> DomainJob:
        body = match_request(file, query, traversal, relation_kind, capture_source)
        route = endpoint("match")
        return domain_job(request(self._http, *route, body=body))

    def dependencies(self, file: FileSelector) -> DomainJob:
        body = file_request(file)
        route = endpoint("dependencies")
        return domain_job(request(self._http, *route, body=body))
