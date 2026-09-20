import json

import httpx

from facts_tool.rest import (
    Client,
    DeclarationLocation,
    SymbolReference,
    VariableReference,
)

from .v2_helpers import job


def test_graph_and_local_variable_tracking_are_distinct_typed_requests():
    requests = []

    def handle(request):
        requests.append(request)
        operation = request.url.path.split("/")[-2]
        return httpx.Response(202, json=job(operation=operation))

    with Client("http://test", transport=httpx.MockTransport(handle)) as api:
        api.callgraphs.create(
            root=SymbolReference(qualified_name="Service::run"),
            direction="callers",
            max_depth=3,
        )
        api.variable_flow.create(
            function=SymbolReference(symbol_id="sym-1"),
            variable=VariableReference(
                "request", DeclarationLocation("Service.cpp", 42, 9)
            ),
            interprocedural=True,
            max_call_depth=4,
        )
    graph, flow = [json.loads(r.content) for r in requests]
    assert graph == {
        "root": {"qualified_name": "Service::run"},
        "direction": "callers",
        "max_depth": 3,
    }
    assert flow["variable"]["declaration"] == {
        "path": "Service.cpp",
        "line": 42,
        "column": 9,
    }
    assert flow["function"] == {"symbol_id": "sym-1"}
    assert requests[1].url.path == "/api/v2/variable-flow/job"


def test_callgraph_nodes_are_typed_and_paged_independently():
    requests = []
    node = {
        "symbol_id": "sym-1",
        "qualified_name": "run",
        "usr": "usr-run",
        "definition": True,
        "external": False,
        "file_id": "1",
        "line": 1,
        "column": 1,
        "unresolved_calls": 0,
        "pointer_calls": 0,
    }

    def handle(request):
        requests.append(request)
        return httpx.Response(200, json={"items": [node], "next_cursor": None})

    with Client("http://test", transport=httpx.MockTransport(handle)) as api:
        nodes = api.callgraphs.nodes("job-1")
        assert requests == []
        assert nodes.collect()[0].qualified_name == "run"
    assert requests[0].method == "GET"
    assert requests[0].url.params["collection"] == "nodes"
