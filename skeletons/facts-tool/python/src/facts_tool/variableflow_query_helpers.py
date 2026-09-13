from collections.abc import Mapping

from .errors import fail
from .variableflow_models import Edge, Node


def node_id(node: Node | int, nodes: Mapping[int, Node]) -> int:
    value = node.id if isinstance(node, Node) else node
    if not isinstance(value, int) or isinstance(value, bool):
        fail("E_SOURCE", "node must be a variable-flow Node or integer id")
    if value not in nodes:
        fail("E_SOURCE", f"variable-flow node {value} not found")
    if isinstance(node, Node) and nodes[value] is not node:
        fail("E_SOURCE", f"node {value} does not belong to this graph")
    return value


def edges_for(
    edges: tuple[Edge, ...],
    node: Node | int,
    kind: str | None,
    side: str,
    nodes: Mapping[int, Node],
) -> tuple[Edge, ...]:
    value = node_id(node, nodes)
    return tuple(
        edge
        for edge in edges
        if getattr(edge, side) == value and (kind is None or edge.kind == kind)
    )
