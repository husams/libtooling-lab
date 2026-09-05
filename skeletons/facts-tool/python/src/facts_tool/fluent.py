from collections.abc import Callable, Sequence

from . import fluent_results
from .executor import Executor
from .queryplan.stages import in_, nodes, out, view, where
from .queryplan.stages_shape import limit, order_by, select
from .queryplan.types import Plan, Pred, Query, Stage
from .result import Result
from .rows import Row


class EntityQuery:
    def __init__(
        self,
        executor: Executor,
        query: Query,
        callbacks: tuple[Callable[[Row], bool], ...] = (),
    ):
        self.executor, self._query, self._callbacks = executor, query, callbacks

    @property
    def plan(self) -> Plan:
        return self._query.plan

    def to_plan(self) -> Plan:
        return self.plan

    def _add(self, stage: Stage) -> "EntityQuery":
        return EntityQuery(self.executor, self._query | stage, self._callbacks)

    def nodes(
        self, pred: Pred | None = None, unknown: str = "exclude"
    ) -> "EntityQuery":
        return self._add(nodes(pred, unknown))

    def where(self, pred: Pred, unknown: str = "exclude") -> "EntityQuery":
        return self._add(where(pred, unknown))

    def view(self, name: str) -> "EntityQuery":
        return self._add(view(name))

    def relation(
        self, name: str, min_depth: int = 1, max_depth: int = 1, inbound: bool = False
    ) -> "EntityQuery":
        stage = (
            in_(name, min_depth, max_depth)
            if inbound
            else out(name, min_depth, max_depth)
        )
        return self._add(stage)

    def select(self, fields: Sequence[str]) -> "EntityQuery":
        return self._add(select(fields))

    def order_by(self, fields: Sequence[str]) -> "EntityQuery":
        return self._add(order_by(fields))

    def limit(self, value: int) -> "EntityQuery":
        return self._add(limit(value))

    def filter(self, callback: Callable[[Row], bool]) -> "EntityQuery":
        return EntityQuery(self.executor, self._query, (*self._callbacks, callback))

    def run(self) -> Result:
        return fluent_results.run(self)

    def all(self) -> list[object]:
        return fluent_results.all_rows(self)

    def names(self) -> list[str]:
        return fluent_results.names(self)

    def count(self) -> int | None:
        return fluent_results.count_rows(self)

    def first(self) -> object | None:
        return fluent_results.first(self)
