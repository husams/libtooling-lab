from .errors import fail
from .execution_context import ExecutionContext
from .filtering import filter_rows
from .queryplan.types import Stage
from .source_exec import enumerate_view
from .state import ExecutionState
from .traversal import traverse


def apply_nodes(state: ExecutionState, stage: Stage, context: ExecutionContext) -> None:
    rows = state.values
    if not context.enumerated:
        if not rows:
            rows = enumerate_view(state, context.loader, context.after_id)
        context.enumerated = True
        if len(rows) > context.budgets.enumeration:
            rows = rows[: context.budgets.enumeration]
            state.truncated = True
        if rows:
            state.cursor = str(rows[-1].get("id", rows[-1]["_key"]))
    state.values, unknown = filter_rows(
        rows, stage.pred, stage.unknown, context.neighbors, context.budgets.traversal
    )
    state.unknown |= unknown


def apply_where(state: ExecutionState, stage: Stage, context: ExecutionContext) -> None:
    state.values, unknown = filter_rows(
        state.values,
        stage.pred,
        stage.unknown,
        context.neighbors,
        context.budgets.traversal,
    )
    state.unknown |= unknown


def apply_traversal(
    state: ExecutionState, stage: Stage, context: ExecutionContext
) -> None:
    if stage.mode == "devirtualized":
        fail(
            "E_CAPABILITY", "devirtualized traversal is unavailable; use dispatch_calls"
        )
    state.values, truncated = traverse(
        state.values,
        stage.relation,
        stage.min_depth,
        stage.max_depth,
        stage.op == "in",
        context.neighbors,
        context.budgets.traversal,
    )
    state.truncated |= truncated
    if state.values:
        state.view = str(state.values[0]["_view"])


def apply_view(state: ExecutionState, stage: Stage, context: ExecutionContext) -> None:
    if state.values and state.view == "symbol":
        state.context_ids = {int(row["id"]) for row in state.values}
    else:
        state.context_ids = None
    state.view = stage.level
    state.values = []
    context.enumerated = False
