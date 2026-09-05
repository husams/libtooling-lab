import json
from collections.abc import Iterator
from dataclasses import dataclass
from typing import Any, cast

from .provenance import PairProvenance
from .rows import Row, public_row


def _safe(value: Any) -> Any:
    if isinstance(value, int) and abs(value) > (1 << 53) - 1:
        return str(value)
    if isinstance(value, dict):
        return {key: _safe(item) for key, item in value.items()}
    if isinstance(value, list):
        return [_safe(item) for item in value]
    return value


@dataclass(frozen=True)
class Result:
    shape: str
    view: str
    values: tuple[Row, ...]
    scalar: int | None
    truncated: bool
    partial: bool
    unknown: bool
    cursor: str | None
    provenance: PairProvenance

    @property
    def nodes(self) -> tuple[Row, ...]:
        return self.values if self.shape == "nodes" else ()

    @property
    def rows(self) -> tuple[Row, ...]:
        return self.values if self.shape == "rows" else ()

    @property
    def paths(self) -> tuple[Row, ...]:
        return self.values if self.shape == "path" else ()

    def __iter__(self) -> Iterator[Row]:
        return iter(self.values)

    def __len__(self) -> int:
        return len(self.values)

    def to_dict(self) -> dict[str, Any]:
        payload: dict[str, Any] = {
            "shape": self.shape,
            "view": self.view,
            "truncated": self.truncated,
            "partial": self.partial,
            "unknown": self.unknown,
            "cursor": self.cursor,
            "provenance": self.provenance.to_dict(),
        }
        if self.shape == "scalar":
            payload["scalar"] = self.scalar
        else:
            payload[{"nodes": "nodes", "rows": "rows", "path": "paths"}[self.shape]] = [
                public_row(row) for row in self.values
            ]
        return cast(dict[str, Any], _safe(payload))

    def to_json(self) -> str:
        return json.dumps(self.to_dict(), ensure_ascii=False, sort_keys=True)
