from collections.abc import Iterator
from dataclasses import dataclass, field
from typing import Any

from .provenance import PairProvenance
from .result_payload import payload, serialized
from .result_stream import ResultData, StreamFactory
from .rows import Row
from .state import ExecutionState

_LAZY_FIELDS = frozenset(
    {"shape", "view", "values", "scalar", "truncated", "partial", "unknown", "cursor"}
)


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
    _data: ResultData | None = field(
        default=None, init=False, repr=False, compare=False
    )

    def __getattribute__(self, name: str) -> Any:
        if name in _LAZY_FIELDS:
            data: ResultData | None = object.__getattribute__(self, "_data")
            if data is not None:
                if name == "values":
                    return data.materialize()
                state = data.state if name in {"shape", "view"} else data.metadata()
                return getattr(state, name)
        return object.__getattribute__(self, name)

    @classmethod
    def lazy(
        cls,
        shape: str,
        view: str,
        factory: StreamFactory,
        provenance: PairProvenance,
    ) -> "Result":
        data = ResultData(ExecutionState(view=view, shape=shape), None, factory)
        result = cls(shape, view, (), None, False, False, False, None, provenance)
        object.__setattr__(result, "_data", data)
        return result

    @property
    def nodes(self) -> tuple[Row, ...]:
        return self.values if self.shape == "nodes" else ()

    @property
    def rows(self) -> tuple[Row, ...]:
        return self.values if self.shape == "rows" else ()

    @property
    def paths(self) -> tuple[Row, ...]:
        return self.values if self.shape == "path" else ()

    def materialize(self) -> "Result":
        _ = self.values
        return self

    def __iter__(self) -> Iterator[Row]:
        return self._data.iterate() if self._data is not None else iter(self.values)

    def __len__(self) -> int:
        return len(self.values)

    def to_dict(self) -> dict[str, Any]:
        return payload(self)

    def to_json(self) -> str:
        return serialized(self)
