from collections.abc import Iterator

from .execution_context import ExecutionContext
from .executor_dispatch import execute
from .queryplan.types import Plan
from .rows import Row
from .shaping import distinct_rows, order_rows, select_rows
from .source_exec import resolve_source
from .state import ExecutionState
from .stream_rows import bounded, filtered, limited
from .stream_sources import enumerate_nodes


def supports_streaming(plan: Plan) -> bool:
    if plan.source.kind != "codebase":
        return False
    remaining = plan.stages
    while remaining and remaining[0].op == "view":
        remaining = remaining[1:]
    return all(
        stage.op
        in {"nodes", "where", "select", "limit", "count", "order_by", "distinct"}
        for stage in remaining
    )


def execute_stream(
    plan: Plan, ctx: ExecutionContext, state: ExecutionState, cap: int
) -> Iterator[Row]:
    if not supports_streaming(plan):
        complete = execute(plan, ctx)
        state.__dict__.update(complete.__dict__)
        try:
            yield from bounded(iter(state.values), state, cap)
        finally:
            state.values = []
        return
    initial = resolve_source(plan.source, ctx.loader)
    state.__dict__.update(initial.__dict__)
    rows = iter(initial.values)
    streams: list[Iterator[Row]] = [rows]
    try:
        for index, stage in enumerate(plan.stages):
            if stage.op == "view":
                state.view = stage.level
                rows = iter(())
            elif stage.op == "nodes":
                if not ctx.enumerated:
                    rows = enumerate_nodes(state, ctx, plan.stages[index:])
                    streams.append(rows)
                    ctx.enumerated = True
                rows = filtered(rows, stage, state, ctx)
            elif stage.op == "where":
                rows = filtered(rows, stage, state, ctx)
            elif stage.op == "select":
                rows = projected(rows, state.view, stage.fields)
                state.shape = "rows"
            elif stage.op == "limit":
                rows = limited(rows, stage.n)
            elif stage.op == "count":
                count = sum(1 for _ in rows)
                state.scalar = None if state.truncated else count
                state.shape = "scalar"
                rows = iter(())
            elif stage.op == "order_by":
                rows = iter(order_rows(list(rows), stage.fields))
            elif stage.op == "distinct":
                rows = iter(distinct_rows(list(rows)))
            streams.append(rows)
        yield from bounded(rows, state, cap)
    finally:
        for stream in reversed(streams):
            close = getattr(stream, "close", None)
            if close is not None:
                close()


def projected(rows: Iterator[Row], view: str, fields: tuple[str, ...]) -> Iterator[Row]:
    for row in rows:
        yield from select_rows([row], view, fields)
