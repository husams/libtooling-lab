from pathlib import Path

import pytest
from support.variableflow_native import analyse, prepare_variable_flow

from facts_tool import FactsToolError, open_variable_flow


@pytest.fixture
def native_flow(tmp_path: Path) -> tuple[Path, Path]:
    project, facts, executable = prepare_variable_flow(tmp_path / "flow")
    output = tmp_path / "variable-flow.db"
    analyse(
        executable,
        tmp_path / "flow",
        project,
        facts,
        output,
        "variable_flow::root",
        "result",
    )
    return output, executable


def test_native_graph_queries_and_lifecycle(native_flow, tmp_path: Path) -> None:
    path, executable = native_flow
    with open_variable_flow(path) as flows:
        run = flows.get(1)
        graph = run.graph
        result = graph.nodes(
            variable_usr=run.root_variable, function_usr=run.root_function
        )
        assert result
        assert {node.kind for node in graph.reads(variable_usr=run.root_variable)} >= {
            "read",
            "update",
        }
        update = next(node for node in result if node.kind == "update")
        assert update in graph.reads(variable_usr=run.root_variable)
        assert update in graph.writes(variable_usr=run.root_variable)
        data_read = next(
            node for node in graph.reads() if graph.incoming(node, kind="data")
        )
        assert graph.node(graph.incoming(data_read, kind="data")[0].source)
        callsite_edges = [edge for edge in run.edges if edge.callsite]
        assert callsite_edges
        assert all(graph.node(edge.callsite).kind == "call" for edge in callsite_edges)
        assert any(edge.kind == "return" for edge in run.edges)
        assert any(edge.kind == "argument-ref" for edge in run.edges)

        parameter = next(
            node for node in graph.nodes(kind="parameter") if node.variable_usr
        )
        assert graph.nodes(
            variable_usr=parameter.variable_usr, function_usr=parameter.function_usr
        )
        graph_after_close = graph
    assert graph_after_close.nodes(kind="read")

    with pytest.raises(FactsToolError, match="E_SOURCE"):
        flows.get(1)

    boundary_output = tmp_path / "boundaries.variable-flow.db"
    analyse(
        executable,
        tmp_path / "flow",
        path.parent / "flow/project.db",
        path.parent / "flow/facts.db",
        boundary_output,
        "variable_flow::external_boundary",
        "seed",
    )
    with open_variable_flow(boundary_output) as flows:
        boundary_run = flows.get(1)
        assert boundary_run.graph.boundaries(reason="external")
        assert not boundary_run.graph.boundaries(reason="indirect")
