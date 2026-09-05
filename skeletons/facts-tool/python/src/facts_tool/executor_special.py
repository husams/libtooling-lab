from .execution_context import ExecutionContext
from .pathing import attach_evidence, find_paths
from .queryplan.types import Stage
from .rows import row_key
from .state import ExecutionState
from .type_use import direct_type_use
from .witness_budget import cap_witnesses


def apply_sites(state: ExecutionState, context: ExecutionContext) -> None:
    edges = {
        (row["source_id"], row["destination_id"], row["kind_id"], row["position"])
        for row in state.values
    }
    state.values = [
        row
        for row in context.loader.load("site")
        if (row["source_id"], row["destination_id"], row["kind_id"], row["position"])
        in edges
    ]
    state.view = "site"


def apply_set(state: ExecutionState, other: ExecutionState, op: str) -> None:
    if state.shape != "nodes" or other.shape != "nodes" or state.view != other.view:
        from .errors import fail

        fail("E_SETOP", "set operands must have the same node view")
    left = {row_key(row): row for row in state.values}
    right = {row_key(row): row for row in other.values}
    if op == "union":
        left.update(right)
    elif op == "intersect":
        left = {key: row for key, row in left.items() if key in right}
    else:
        left = {key: row for key, row in left.items() if key not in right}
    state.values = list(left.values())
    state.truncated |= other.truncated
    state.partial |= other.partial
    state.unknown |= other.unknown


def apply_path(
    state: ExecutionState,
    targets: ExecutionState,
    stage: Stage,
    context: ExecutionContext,
) -> None:
    state.values, truncated = find_paths(
        state.values,
        targets.values,
        stage.relation,
        stage.min_depth,
        stage.max_depth,
        stage.inbound,
        context.neighbors,
        context.budgets.path_expansion,
    )
    state.values, reconstruction_truncated = cap_witnesses(
        state.values, context.budgets.witness_reconstruction
    )
    attach_evidence(state.values, context.loader, stage.relation)
    if stage.n:
        state.values = state.values[: stage.n]
    state.shape, state.view = "path", "path"
    state.truncated |= truncated or reconstruction_truncated or targets.truncated


def apply_reverse_type_use(
    state: ExecutionState, stage: Stage, context: ExecutionContext
) -> None:
    state.values = direct_type_use(state.values, context.loader)
    state.shape, state.view = "path", "path"
    state.partial |= stage.max_depth > 1
