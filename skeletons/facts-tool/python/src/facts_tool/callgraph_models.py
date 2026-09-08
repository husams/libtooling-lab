from dataclasses import dataclass
from typing import Any, cast


def _safe(value: Any) -> Any:
    if isinstance(value, int) and abs(value) > (1 << 53) - 1:
        return str(value)
    if isinstance(value, dict):
        return {key: _safe(item) for key, item in value.items()}
    if isinstance(value, list):
        return [_safe(item) for item in value]
    return value


@dataclass(frozen=True)
class CallGraphSymbol:
    symbol_id: int
    usr: str
    qualified_name: str
    file_id: int
    file: str | None
    line: int | None
    column: int | None

    def to_dict(self) -> dict[str, Any]:
        return cast(dict[str, Any], _safe(self.__dict__.copy()))


@dataclass(frozen=True)
class CallGraphSite:
    file_id: int
    file: str | None
    line: int | None
    column: int | None
    offset: int
    receiver_type_id: int | None
    certainty: int | None
    enriched: bool = False

    def to_dict(self) -> dict[str, Any]:
        return cast(dict[str, Any], _safe(self.__dict__.copy()))


@dataclass(frozen=True)
class CallGraphRoot:
    symbol: CallGraphSymbol
    usr: str

    @property
    def symbol_id(self) -> int:
        return self.symbol.symbol_id

    def to_dict(self) -> dict[str, Any]:
        return cast(
            dict[str, Any], _safe({"symbol": self.symbol.to_dict(), "usr": self.usr})
        )


@dataclass(frozen=True)
class CallGraphTarget(CallGraphRoot):
    pass


@dataclass(frozen=True)
class CallGraphEdge:
    source: CallGraphSymbol
    target: CallGraphSymbol
    kind_id: int
    kind: str
    semantic_kind: str
    position: int
    file_id: int
    file: str | None
    line: int | None
    column: int | None
    offset: int
    depth: int
    cycle: bool
    site: CallGraphSite | None

    @property
    def sites(self) -> tuple[CallGraphSite, ...]:
        return (self.site,) if self.site else ()

    def to_dict(self) -> dict[str, Any]:
        value = self.__dict__.copy()
        value["source"] = self.source.to_dict()
        value["target"] = self.target.to_dict()
        value["site"] = self.site.to_dict() if self.site else None
        return cast(dict[str, Any], _safe(value))
