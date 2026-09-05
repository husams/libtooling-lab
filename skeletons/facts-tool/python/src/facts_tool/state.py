from dataclasses import dataclass, field

from .rows import Row


@dataclass
class ExecutionState:
    view: str = "symbol"
    shape: str = "nodes"
    values: list[Row] = field(default_factory=list)
    scalar: int | None = None
    truncated: bool = False
    partial: bool = False
    unknown: bool = False
    cursor: str | None = None
    context_ids: set[int] | None = None
