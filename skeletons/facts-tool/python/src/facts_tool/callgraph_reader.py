import sqlite3
from collections.abc import Mapping

from .callgraph_build import decode_run
from .callgraph_db import run
from .callgraph_result import CallGraphRun
from .errors import fail
from .paths import FileResolver
from .provenance import PairProvenance


def _limit(value: int, name: str) -> int:
    if not isinstance(value, int) or isinstance(value, bool) or value <= 0:
        fail("E_LIMIT", f"{name} must be a positive integer")
    return value


class CallGraphReader:
    def __init__(
        self,
        facts: sqlite3.Connection,
        project: sqlite3.Connection,
        provenance: PairProvenance,
    ):
        self.facts, self.files, self.provenance = (
            facts,
            FileResolver(project),
            provenance,
        )

    def _require_supported(self) -> None:
        if self.provenance.facts.schema.user_version not in (12, 13):
            fail(
                "E_CAPABILITY",
                "persisted call graph runs require facts schema 12 or 13",
            )

    def list(
        self, *, limit: int = 100, after: int | None = None
    ) -> tuple[CallGraphRun, ...]:
        self._require_supported()
        bound = _limit(limit, "limit")
        if after is not None and (
            not isinstance(after, int) or isinstance(after, bool) or after < 0
        ):
            fail("E_LIMIT", "after must be a non-negative run id")
        sql = "SELECT run_id FROM callgraph_run "
        values: tuple[object, ...] = ()
        if after is not None:
            sql += "WHERE run_id>? "
            values = (after,)
        sql += "ORDER BY run_id LIMIT ?"
        ids = [int(row[0]) for row in self.facts.execute(sql, (*values, bound))]
        return tuple(self.get(run_id, limit=1) for run_id in ids)

    def latest(self) -> CallGraphRun | None:
        self._require_supported()
        row = self.facts.execute(
            "SELECT run_id FROM callgraph_run ORDER BY run_id DESC LIMIT 1"
        ).fetchone()
        return self.get(int(row[0])) if row else None

    def get(
        self,
        run_id: int,
        *,
        limit: int = 1000,
        offset: int = 0,
        cursors: Mapping[str, int] | None = None,
    ) -> CallGraphRun:
        self._require_supported()
        bound = _limit(limit, "limit")
        if not isinstance(run_id, int) or isinstance(run_id, bool) or run_id < 1:
            fail("E_SOURCE", "run_id must be a positive integer")
        if not isinstance(offset, int) or isinstance(offset, bool) or offset < 0:
            fail("E_LIMIT", "offset must be a non-negative integer")
        names = {"roots", "targets", "edges", "frontier", "recovery"}
        if cursors is None:
            page_cursors = dict.fromkeys(names, offset)
        else:
            if set(cursors) - names:
                fail("E_LIMIT", "cursors contain an unknown collection")
            page_cursors = dict.fromkeys(names, 0)
            for name, cursor in cursors.items():
                if (
                    not isinstance(cursor, int)
                    or isinstance(cursor, bool)
                    or cursor < 0
                ):
                    fail("E_LIMIT", f"{name} cursor must be non-negative")
                page_cursors[name] = cursor
        row = run(self.facts, run_id)
        if row is None:
            fail("E_SOURCE", f"call graph run {run_id} not found")
        return decode_run(
            self.facts, self.files, self.provenance, row, bound, page_cursors
        )
