from .execution_context import ExecutionContext
from .fields import FIELDS
from .queryplan.types import Stage
from .shaping import distinct_rows, order_rows, select_rows
from .state import ExecutionState


def apply_shape(state: ExecutionState, stage: Stage, context: ExecutionContext) -> bool:
    if stage.op == "select":
        state.values = select_rows(state.values, state.view, stage.fields)
        state.shape = "rows"
    elif stage.op == "distinct":
        state.values = distinct_rows(state.values)
    elif stage.op == "order_by":
        if state.shape == "nodes":
            unknown = [
                field
                for field in stage.fields
                if field not in FIELDS.get(state.view, set())
            ]
            if unknown:
                from .errors import fail

                fail("E_FIELD", f"cannot order by: {', '.join(unknown)}")
        state.values = order_rows(state.values, stage.fields)
    elif stage.op == "limit":
        state.values = state.values[: stage.n]
    elif stage.op == "count":
        state.scalar = None if state.truncated else len(state.values)
        state.values = []
        state.shape = "scalar"
    elif stage.op == "rank":
        state.values.sort(key=lambda row: (int(row.get("length", 0)), str(row["_key"])))
        if stage.n:
            state.values = state.values[: stage.n]
    else:
        return False
    return True
