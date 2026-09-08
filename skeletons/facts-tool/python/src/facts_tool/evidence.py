from __future__ import annotations

from typing import Any

from .evidence_expressions import ExpressionEvidenceQuery
from .result import Result
from .source_evidence import prepare_source_regions
from .view_evidence import load_source_regions


class EvidenceQuery(ExpressionEvidenceQuery):
    """Read-only access to schema13 expression and source evidence."""

    def source_regions(
        self,
        ref: str | None = None,
        *,
        include_text: bool = False,
        max_bytes: int = 64 * 1024,
        limit: int | None = None,
        after_id: int | str | None = None,
    ) -> Result:
        self._require_schema13()
        if max_bytes < 1:
            from .errors import fail

            fail("E_LIMIT", "max_bytes must be positive")
        symbol_id = self._sqlite_symbol_id(self._symbol(ref)) if ref else None
        rows = load_source_regions(
            self.executor.loader.facts, self.executor.loader.files
        )
        if symbol_id is not None:
            rows = [row for row in rows if int(row["symbol_id"]) == symbol_id]
        cap = self.executor.budgets.result_cap if limit is None else limit
        if cap < 1:
            from .errors import fail

            fail("E_LIMIT", "limit must be positive")
        page, truncated, cursor = self._window(rows, cap, after_id)
        page, unknown, partial = prepare_source_regions(
            self.executor, symbol_id, include_text, max_bytes, page
        )
        return Result(
            "rows",
            "source_region",
            tuple(page),
            None,
            truncated,
            partial,
            unknown,
            cursor,
            self.executor.provenance,
        )

    def source_sections(self, *args: Any, **kwargs: Any) -> Result:
        return self.source_regions(*args, **kwargs)

    def definition_regions(self, *args: Any, **kwargs: Any) -> Result:
        return self.source_regions(*args, **kwargs)
