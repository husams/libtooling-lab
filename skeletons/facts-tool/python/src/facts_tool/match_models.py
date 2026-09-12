from collections.abc import Mapping
from dataclasses import dataclass
from types import MappingProxyType
from typing import Any


@dataclass(frozen=True)
class MatchLocation:
    path: str
    line: int
    column: int
    offset: int

    def to_dict(self) -> dict[str, Any]:
        return {
            "path": self.path,
            "line": self.line,
            "column": self.column,
            "offset": self.offset,
        }


@dataclass(frozen=True)
class MatchRange:
    path: str
    offset: int
    size: int

    def to_dict(self) -> dict[str, Any]:
        return {"path": self.path, "offset": self.offset, "size": self.size}


@dataclass(frozen=True)
class MatchBinding:
    node_kind: str
    name: str | None
    usr: str | None
    location: MatchLocation | None
    range: MatchRange | None
    location_unavailable_reason: str | None

    def to_dict(self) -> dict[str, Any]:
        return {
            "node_kind": self.node_kind,
            "name": self.name,
            "usr": self.usr,
            "location": self.location.to_dict() if self.location else None,
            "range": self.range.to_dict() if self.range else None,
            "location_unavailable_reason": self.location_unavailable_reason,
        }


@dataclass(frozen=True)
class MatchResult:
    translation_unit: str
    relation_kind: str | None
    bindings: Mapping[str, MatchBinding]

    def __post_init__(self) -> None:
        object.__setattr__(self, "bindings", MappingProxyType(dict(self.bindings)))

    def to_dict(self) -> dict[str, Any]:
        return {
            "translation_unit": self.translation_unit,
            "relation_kind": self.relation_kind,
            "bindings": {key: value.to_dict() for key, value in self.bindings.items()},
        }
