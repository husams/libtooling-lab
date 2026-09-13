from dataclasses import FrozenInstanceError

import pytest

from facts_tool import (
    Boundary,
    Edge,
    Location,
    Node,
    VariableFlowRun,
)
from facts_tool.errors import FactsToolError


def _run() -> VariableFlowRun:
    location = Location("/tmp/flow.cpp", 1, 1, 0)
    nodes = (
        Node(1, "write", "f", "local", "value", "int", location),
        Node(2, "update", "f", "local", "value", "int", location),
        Node(3, "read", "f", "local", "value", "int", location),
        Node(4, "parameter", "f", "param", "input", "int", location),
        Node(5, "call", "f", "", "callee", "", location),
        Node(6, "write", "g", "param", "input", "int", location, depth=1),
    )
    return VariableFlowRun(
        1,
        "now",
        "/tmp/project.db",
        "/tmp/facts.db",
        "f",
        "value",
        ("/tmp/flow.cpp",),
        None,
        None,
        "engine",
        "assumptions",
        "f",
        "local",
        "complete",
        nodes,
        (Edge(1, 2, "data"), Edge(2, 3, "data"), Edge(5, 6, "effect", 5)),
        (Boundary(5, "external", "callee", 0),),
    )


def test_graph_queries_keep_occurrences_and_callsites() -> None:
    graph = _run().graph

    assert [node.id for node in graph.nodes(variable_usr="local")] == [1, 2, 3]
    assert [node.id for node in graph.reads(variable_usr="local")] == [2, 3]
    assert [node.id for node in graph.writes(variable_usr="local")] == [1, 2]
    assert [node.id for node in graph.nodes(variable_usr="param")] == [4, 6]
    assert graph.node(4).kind == "parameter"
    assert graph.outgoing(5)[0] == Edge(5, 6, "effect", 5)
    assert graph.incoming(3)[0].source == 2
    assert graph.boundaries(5)[0].reason == "external"


def test_graph_queries_reject_unknown_nodes() -> None:
    graph = _run().graph

    assert graph.nodes(kind={"read", "update"})[0].id == 2
    with pytest.raises(FactsToolError, match="E_SOURCE"):
        graph.outgoing(99)
    with pytest.raises(FactsToolError, match="E_SOURCE"):
        graph.node(99)
    foreign = Node(
        1,
        "read",
        "other",
        "other",
        "value",
        "int",
        Location("/tmp/other.cpp", 1, 1, 0),
    )
    with pytest.raises(FactsToolError, match="E_SOURCE"):
        graph.incoming(foreign)
    equal_foreign = Node(
        1, "write", "f", "local", "value", "int", Location("/tmp/flow.cpp", 1, 1, 0)
    )
    with pytest.raises(FactsToolError, match="E_SOURCE"):
        graph.incoming(equal_foreign)


def test_run_models_remain_frozen_and_serializable() -> None:
    run = _run()
    assert run.to_dict()["nodes"][0]["kind"] == "write"
    with pytest.raises(FrozenInstanceError):
        run.nodes = ()  # type: ignore[misc]
