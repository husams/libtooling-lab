from collections.abc import Iterator
from typing import Any

from .budgets import Budgets
from .catalog import RELATION_NAMES, VIEWS
from .execution_context import ExecutionContext
from .neighbors import Neighbors
from .provenance import PairProvenance
from .queryplan.serialize import canonical_json, plan_to_dict
from .queryplan.types import Plan
from .queryplan.validation import validate
from .result import Result
from .result_layout import result_layout
from .rows import Row
from .state import ExecutionState
from .stream_execute import execute_stream
from .view_loader import ViewLoader


class Executor:
    def __init__(
        self,
        loader: ViewLoader,
        provenance: PairProvenance,
        budgets: Budgets | None = None,
        *,
        lazy: bool = True,
    ):
        self.lazy = lazy
        self.loader = loader
        self.provenance = provenance
        self.budgets = budgets or Budgets()
        self.neighbors = Neighbors(loader.facts, loader)

    def run(
        self,
        plan: Plan,
        after_id: str | int | None = None,
        result_cap: int | None = None,
        *,
        lazy: bool | None = None,
    ) -> Result:
        validate(plan)
        if any(stage.max_depth > self.budgets.max_depth for stage in plan.stages):
            from .errors import fail

            fail("E_BUDGET", "plan depth exceeds the executor budget")
        cap = self.budgets.result_cap if result_cap is None else result_cap
        if cap < 1:
            from .errors import fail

            fail("E_LIMIT", "result_cap must be positive")

        shape, view = result_layout(plan)

        def factory() -> tuple[Iterator[Row], ExecutionState]:
            context = ExecutionContext(
                self.loader, self.neighbors, self.budgets, after_id
            )
            state = ExecutionState(shape=shape, view=view)
            return execute_stream(plan, context, state, cap), state

        result = Result.lazy(shape, view, factory, self.provenance)
        return result if (self.lazy if lazy is None else lazy) else result.materialize()

    def explain(self, plan: Plan) -> dict[str, Any]:
        validate(plan)
        shape = "nodes"
        for stage in plan.stages:
            shape = {
                "select": "rows",
                "count": "scalar",
                "path": "path",
                "reverse_type_use": "path",
            }.get(stage.op, shape)
        return {
            "canonical_plan": canonical_json(plan),
            "plan": plan_to_dict(plan),
            "shape": shape,
            "budgets": self.budgets.to_dict(),
            "relations": list(RELATION_NAMES),
            "views": sorted(VIEWS),
            "provenance": self.provenance.to_dict(),
        }
