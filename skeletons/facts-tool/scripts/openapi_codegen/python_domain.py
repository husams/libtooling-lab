"""Generate typed resource operations alongside compatibility client methods."""

from .python_routes import operations

OPERATIONS = {"findSymbols", "indexStatus", "extract", "match", "dependencies"}
SIGNATURE = '''    def find_symbols(
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
'''


def resource_clients(spec: dict) -> dict[str, str]:
    if not OPERATIONS <= operations(spec).keys():
        raise ValueError("The contract is missing required resource operations")
    result = {}
    for asynchronous in (False, True):
        prefix, http = ("async_", "AsyncClient") if asynchronous else ("", "Client")
        name = "AsyncResourceOperations" if asynchronous else "ResourceOperations"
        methods = SIGNATURE
        if asynchronous:
            methods = methods.replace("    def ", "    async def ")
            methods = methods.replace("request(self", "await async_request(self")
        result[f"python/src/facts_tool/rest/generated/{prefix}resources.py"] = (
            '"""Generated from the OpenAPI resource operations; do not edit."""\n\n'
            "import httpx\n\n"
            "from ..domain_decoding import domain_job, index_status, symbol_page\n"
            "from ..domain_models import DomainJob, FileSelector, IndexStatus, SymbolPage\n"
            "from ..domain_options import Traversal, extraction_request, match_request\n"
            "from ..domain_requests import file_request, symbol_route\n"
            "from ..endpoints import endpoint\n"
            f"from ..transport import {prefix}request\n\n\n"
            f"class {name}:\n    _http: httpx.{http}\n\n" + methods
        )
    return result
