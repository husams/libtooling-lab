from collections.abc import Iterable, Iterator
from itertools import islice

from .execution_context import ExecutionContext
from .filtering import filter_rows
from .queryplan.types import Stage
from .rows import Row, row_cursor
from .state import ExecutionState


def filtered(
    rows: Iterable[Row], stage: Stage, state: ExecutionState, ctx: ExecutionContext
) -> Iterator[Row]:
    for row in rows:
        matches, unknown = filter_rows(
            [row], stage.pred, stage.unknown, ctx.neighbors, ctx.budgets.traversal
        )
        state.unknown |= unknown
        yield from matches


def bounded(
    rows: Iterable[Row], state: ExecutionState, cap: int, *, enumeration: bool = False
) -> Iterator[Row]:
    marker = None
    for index, row in enumerate(rows):
        if index >= cap:
            state.truncated = True
            if marker is not None:
                state.cursor = str(marker)
            return
        marker = row_cursor(row)
        if enumeration and marker is not None:
            state.cursor = str(marker)
        yield row


def source_rows(state: ExecutionState, ctx: ExecutionContext) -> Iterator[Row]:
    rows = ctx.loader.iter(state.view)
    try:
        for row in rows:
            if (
                state.context_ids is not None
                and state.view != "symbol"
                and row.get("owner_id", row.get("symbol_id")) not in state.context_ids
            ):
                continue
            if ctx.after_id is not None and not after(row, ctx.after_id):
                continue
            yield row
    finally:
        close = getattr(rows, "close", None)
        if close is not None:
            close()


def after(row: Row, marker: str | int) -> bool:
    try:
        boundary = int(marker)
    except ValueError:
        return str(row["_key"]) > str(marker)
    return isinstance(row.get("id"), int) and int(row["id"]) > boundary


def limited(rows: Iterable[Row], count: int) -> Iterator[Row]:
    yield from islice(rows, count)
