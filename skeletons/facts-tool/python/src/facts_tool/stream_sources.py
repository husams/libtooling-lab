from collections.abc import Iterator

from .execution_context import ExecutionContext
from .queryplan.types import Pred, Stage
from .rows import Row
from .state import ExecutionState
from .stream_rows import bounded, source_rows
from .symbol_predicates import compile_symbol_predicate
from .symbol_stream import stream_symbol_nodes


def enumerate_nodes(
    state: ExecutionState, ctx: ExecutionContext, stages: tuple[Stage, ...]
) -> Iterator[Row]:
    if state.view == "symbol":
        predicates = []
        for stage in stages:
            if stage.op not in {"nodes", "where"}:
                break
            if compile_symbol_predicate(stage.pred) is None:
                break
            if stage.pred is not None:
                predicates.append(stage.pred)
        rows = stream_symbol_nodes(
            ctx.loader,
            Pred("all_of", kids=tuple(predicates)),
            ctx.after_id,
            ctx.budgets.enumeration,
            state,
        )
        if rows is not None:
            try:
                yield from rows
            finally:
                close = getattr(rows, "close", None)
                if close is not None:
                    close()
            return
    rows = source_rows(state, ctx)
    try:
        yield from bounded(rows, state, ctx.budgets.enumeration, enumeration=True)
    finally:
        close = getattr(rows, "close", None)
        if close is not None:
            close()
