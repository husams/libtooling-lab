from collections.abc import Callable, Iterator

from .rows import Row
from .state import ExecutionState

StreamFactory = Callable[[], tuple[Iterator[Row], ExecutionState]]


class ResultData:
    def __init__(
        self,
        state: ExecutionState,
        values: tuple[Row, ...] | None,
        factory: StreamFactory | None = None,
    ):
        self.state, self.values, self.factory = state, values, factory
        self.complete = values is not None
        self.generation = 0

    def iterate(self) -> Iterator[Row]:
        if self.values is not None:
            yield from self.values
            return
        assert self.factory is not None
        self.generation += 1
        generation = self.generation
        self.complete = False
        rows, state = self.factory()
        self.state = state
        try:
            yield from rows
        finally:
            close = getattr(rows, "close", None)
            if close is not None:
                close()
        if generation == self.generation:
            self.state, self.complete = state, True

    def materialize(self) -> tuple[Row, ...]:
        if self.values is None:
            self.values = tuple(self.iterate())
        return self.values

    def metadata(self) -> ExecutionState:
        if not self.complete:
            self.materialize()
        return self.state
