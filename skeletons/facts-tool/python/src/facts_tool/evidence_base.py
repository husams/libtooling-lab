from __future__ import annotations

from typing import TYPE_CHECKING

from .errors import fail
from .result import Result
from .rows import Row
from .view_symbols import lookup_symbol

if TYPE_CHECKING:
    from .executor import Executor


class EvidenceBase:
    def __init__(self, executor: Executor):
        self.executor = executor

    def _require_schema13(self) -> None:
        schema = self.executor.provenance.facts.schema
        if schema.user_version < 13:
            fail(
                "E_CAPABILITY",
                "expression and source evidence requires facts schema 13",
            )
        if not {"expression_occurrence", "source_region"} <= set(schema.tables):
            fail("E_CAPABILITY", "facts schema does not expose evidence tables")

    def _symbol(self, ref: str) -> Row:
        matches = lookup_symbol(
            self.executor.loader.facts, self.executor.loader.files, ref
        )
        if not matches:
            fail("E_SOURCE", f"symbol {ref!r} was not found")
        if len(matches) != 1:
            identities = ", ".join(str(row["identity"]) for row in matches[:8])
            fail("E_IDENTITY", f"symbol {ref!r} is ambiguous: {identities}")
        return matches[0]

    @staticmethod
    def _sqlite_symbol_id(row: Row) -> int:
        value = int(row["_db_id"])
        return value if value < (1 << 63) else value - (1 << 64)

    def _page(
        self,
        rows: list[Row],
        view: str,
        limit: int | None,
        after_id: int | str | None,
        unknown: bool = False,
        partial: bool = False,
    ) -> Result:
        if limit is not None and limit < 1:
            fail("E_LIMIT", "limit must be positive")
        cap = self.executor.budgets.result_cap if limit is None else limit
        if cap < 1:
            fail("E_LIMIT", "result cap must be positive")
        rows, truncated, cursor = self._window(rows, cap, after_id)
        values = tuple(rows)
        return Result(
            "rows",
            view,
            values,
            None,
            truncated,
            partial,
            unknown,
            cursor,
            self.executor.provenance,
        )

    def _window(
        self, rows: list[Row], cap: int, after_id: int | str | None
    ) -> tuple[list[Row], bool, str | None]:
        if after_id is not None:
            try:
                boundary = int(after_id)
            except (TypeError, ValueError):
                fail("E_LIMIT", "after_id must be a numeric evidence id")
            rows = [row for row in rows if int(row["id"]) > boundary]
        truncated = len(rows) > cap
        values = rows[:cap]
        cursor = str(values[-1]["id"]) if truncated and values else None
        return values, truncated, cursor
