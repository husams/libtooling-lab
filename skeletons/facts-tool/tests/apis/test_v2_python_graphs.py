"""Typed Python clients consume native call graphs and local-variable transfers."""

import asyncio
import sys
from pathlib import Path

import pytest
from test_v2_analysis_jobs import analysis_server  # noqa: F401

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "python/src"))
from facts_tool.rest import (
    AsyncClient,
    Client,
    DeclarationLocation,
    SymbolReference,
    VariableReference,
)


@pytest.mark.parametrize("asynchronous", [False, True])
def test_python_graph_and_variable_tracking(analysis_server, asynchronous):  # noqa: F811
    async def scenario():
        api = (AsyncClient if asynchronous else Client)(
            f"http://127.0.0.1:{analysis_server.api.port}"
        )

        async def invoke(method, *args, **kwargs):
            result = method(*args, **kwargs)
            return await result if asynchronous else result

        try:
            graph = await invoke(
                api.callgraphs.create,
                root=SymbolReference(qualified_name="alpha::run"),
                max_depth=4,
            )
            summary = await invoke(graph.wait, timeout=30)
            assert summary.node_count >= 2 and summary.nodes is None
            nodes = await invoke(api.callgraphs.nodes(graph.id, limit=1).collect)
            assert any(
                n.qualified_name == "alpha::leaf" and n.definition for n in nodes
            )
            edges = await invoke(api.callgraphs.edges(graph.id, limit=1).collect)
            assert edges and all(isinstance(e.file_id, str) for e in edges)
            flow = await invoke(
                api.variable_flow.create,
                function=SymbolReference(qualified_name="alpha::run"),
                variable=VariableReference(
                    "tracked", DeclarationLocation("src/main.cpp", 4, 7)
                ),
                interprocedural=True,
                max_call_depth=10,
            )
            summary = await invoke(flow.wait, timeout=30)
            assert summary.node_count > 0 and summary.nodes is None
            flow_edges = await invoke(api.variable_flow.edges(flow.id).collect)
            assert {"argument-copy", "return"} <= {e.kind for e in flow_edges}
            flow_nodes = await invoke(api.variable_flow.nodes(flow.id).collect)
            assert any(n.name == "value" and n.depth == 1 for n in flow_nodes)
        finally:
            await api.aclose() if asynchronous else api.close()

    asyncio.run(scenario())
