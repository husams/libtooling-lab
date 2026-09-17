import sqlite3
from dataclasses import dataclass
from typing import Any, cast

from .callgraph_db import many
from .callgraph_models import _safe
from .ids import SymbolId
from .paths import FileResolver


def pointer_calls(
    db: sqlite3.Connection, run_id: int, limit: int, offset: int = 0
) -> list[sqlite3.Row]:
    return many(
        db,
        "SELECT * FROM callgraph_run_pointer_call_site WHERE run_id=? "
        "ORDER BY source_id,file_id,offset LIMIT ? OFFSET ?",
        (run_id, limit, offset),
    )


@dataclass(frozen=True)
class CallGraphPointerCall:
    source_id: int
    target_id: int | None
    target_name: str | None
    target_usr: str | None
    file_id: int
    file: str | None
    line: int
    column: int
    offset: int
    signature: str
    expression: str

    @property
    def kind(self) -> str:
        return "pointer-call"

    def to_dict(self) -> dict[str, Any]:
        return cast(dict[str, Any], _safe({**self.__dict__, "kind": self.kind}))


def decode(row: sqlite3.Row, files: FileResolver) -> CallGraphPointerCall:
    target = row["target_id"]
    return CallGraphPointerCall(
        SymbolId.unpack(int(row["source_id"])).packed,
        SymbolId.unpack(int(target)).packed if target is not None else None,
        row["target_name"],
        row["target_usr"],
        int(row["file_id"]),
        files.path(int(row["file_id"]), required=False),
        int(row["line"]),
        int(row["col"]),
        int(row["offset"]),
        str(row["signature"]),
        str(row["expression"]),
    )
