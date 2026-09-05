from .errors import fail
from .queryplan.types import Source
from .rows import Row
from .state import ExecutionState
from .view_loader import ViewLoader
from .view_symbols import lookup_symbol


def resolve_source(source: Source, loader: ViewLoader) -> ExecutionState:
    if source.kind == "codebase":
        return ExecutionState()
    matches = lookup_symbol(loader.facts, loader.files, source.ref)
    if not matches:
        fail("E_SOURCE", f"symbol {source.ref!r} was not found")
    if len(matches) > 1:
        choices = ", ".join(str(row["qualified_name"]) for row in matches[:5])
        fail("E_SOURCE", f"symbol {source.ref!r} is ambiguous: {choices}")
    return ExecutionState(values=matches, context_ids={int(matches[0]["id"])})


def enumerate_view(
    state: ExecutionState, loader: ViewLoader, after_id: str | int | None
) -> list[Row]:
    rows = loader.load(state.view)
    if state.context_ids is not None and state.view != "symbol":
        rows = [
            row
            for row in rows
            if row.get("owner_id", row.get("symbol_id")) in state.context_ids
        ]
    if after_id is not None:
        try:
            boundary = int(after_id)
            rows = [
                row
                for row in rows
                if isinstance(row.get("id"), int) and int(row["id"]) > boundary
            ]
        except ValueError:
            rows = [row for row in rows if str(row["_key"]) > str(after_id)]
    return rows
