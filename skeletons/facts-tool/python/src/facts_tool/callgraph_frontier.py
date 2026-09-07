import sqlite3
from dataclasses import dataclass
from typing import Any, cast

from .callgraph_models import CallGraphSymbol, _safe


@dataclass(frozen=True)
class CallGraphFrontier:
    symbol: CallGraphSymbol
    reason: str

    @property
    def symbol_id(self) -> int:
        return self.symbol.symbol_id

    def to_dict(self) -> dict[str, Any]:
        return cast(
            dict[str, Any],
            _safe({"symbol": self.symbol.to_dict(), "reason": self.reason}),
        )


def decode(row: sqlite3.Row, value: CallGraphSymbol) -> CallGraphFrontier:
    return CallGraphFrontier(value, str(row["reason"]))
