from collections.abc import Iterator

from .queryplan.types import Pred
from .rows import Row
from .state import ExecutionState
from .symbol_predicates import compile_symbol_predicate
from .symbol_window import symbol_window
from .view_loader import ViewLoader
from .view_symbols import iter_symbol_query


def stream_symbol_nodes(
    loader: ViewLoader,
    pred: Pred | None,
    after_id: str | int | None,
    limit: int,
    state: ExecutionState,
) -> Iterator[Row] | None:
    predicate = compile_symbol_predicate(pred)
    if predicate is None:
        return None
    window = symbol_window(loader.facts, after_id, limit)
    if window is None:
        return None
    state.truncated |= window.truncated
    if window.cursor is not None:
        state.cursor = window.cursor
    where = f"({window.predicate[0]}) AND ({predicate[0]})"
    params = (*window.predicate[1], *predicate[1])
    return iter_symbol_query(loader.facts, loader.files, where, params)
