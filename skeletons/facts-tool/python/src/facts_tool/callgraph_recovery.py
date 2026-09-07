import sqlite3
from dataclasses import dataclass
from typing import Any, cast

from .callgraph_models import _safe
from .paths import FileResolver


@dataclass(frozen=True)
class CallGraphRecovery:
    tu_file_id: int
    file: str | None
    outcome: str
    diagnostic: str | None

    def to_dict(self) -> dict[str, Any]:
        return cast(dict[str, Any], _safe(self.__dict__.copy()))


def decode(row: sqlite3.Row, files: FileResolver) -> CallGraphRecovery:
    file_id = int(row["tu_file_id"])
    return CallGraphRecovery(
        file_id,
        files.path(file_id, required=False),
        str(row["outcome"]),
        row["diagnostic"],
    )
