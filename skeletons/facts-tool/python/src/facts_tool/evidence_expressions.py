from __future__ import annotations

from typing import Any

from .evidence_base import EvidenceBase
from .expression_source import prepare_expression_occurrences
from .result import Result
from .view_evidence import load_expression_occurrences


class ExpressionEvidenceQuery(EvidenceBase):
    def expression_occurrences(
        self,
        ref: str | None = None,
        *,
        owner: str | None = None,
        target: str | None = None,
        access: str | None = None,
        expression_kind: str | None = None,
        limit: int | None = None,
        after_id: int | str | None = None,
    ) -> Result:
        self._require_schema13()
        owner_ref = owner if owner is not None else ref
        owner_id = (
            self._sqlite_symbol_id(self._symbol(owner_ref)) if owner_ref else None
        )
        target_id = self._sqlite_symbol_id(self._symbol(target)) if target else None
        allowed = {"none", "read", "write", "read_write", "escape", "unknown"}
        if access is not None and access not in allowed:
            from .errors import fail

            fail("E_KIND", f"unknown expression access {access!r}")
        rows = load_expression_occurrences(
            self.executor.loader.facts, self.executor.loader.files
        )
        rows = [
            row
            for row in rows
            if (owner_id is None or row.get("owner_id") == owner_id)
            and (target_id is None or row.get("target_id") == target_id)
            and (access is None or row["access"] == access)
            and (expression_kind is None or row["expression_kind"] == expression_kind)
        ]
        rows, unknown, partial = prepare_expression_occurrences(self.executor, rows)
        return self._page(
            rows,
            "expression_occurrence",
            limit,
            after_id,
            unknown or any(row["access"] == "unknown" for row in rows),
            partial,
        )

    def expressions(self, *args: Any, **kwargs: Any) -> Result:
        return self.expression_occurrences(*args, **kwargs)

    def field_accesses(
        self,
        ref: str,
        *,
        access: str | None = None,
        limit: int | None = None,
        after_id: int | str | None = None,
    ) -> Result:
        return self.expression_occurrences(
            target=ref, access=access, limit=limit, after_id=after_id
        )

    def field_writers(
        self, ref: str, *, limit: int | None = None, after_id: int | str | None = None
    ) -> Result:
        self._require_schema13()
        target_id = self._sqlite_symbol_id(self._symbol(ref))
        rows = load_expression_occurrences(
            self.executor.loader.facts, self.executor.loader.files
        )
        rows = [row for row in rows if row.get("target_id") == target_id]
        rows, _unknown, partial = prepare_expression_occurrences(self.executor, rows)
        rows = [row for row in rows if row["access"] in {"write", "read_write"}]
        return self._page(
            rows,
            "expression_occurrence",
            limit,
            after_id,
            partial=partial or any(row["freshness"] != "current" for row in rows),
        )

    def field_writes(self, *args: Any, **kwargs: Any) -> Result:
        return self.field_writers(*args, **kwargs)

    def ancestors(self, ref: str, max_depth: int = 1) -> list[Any]:
        from .graph import GraphQuery

        return GraphQuery(self.executor).bases(ref, max_depth)
