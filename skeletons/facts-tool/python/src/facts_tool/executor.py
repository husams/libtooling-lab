from typing import Any

from .budgets import Budgets
from .catalog import RELATION_NAMES, VIEWS
from .execution_context import ExecutionContext
from .executor_dispatch import execute
from .neighbors import Neighbors
from .provenance import PairProvenance
from .queryplan.serialize import canonical_json, plan_to_dict
from .queryplan.types import Plan
from .queryplan.validation import validate
from .result import Result
from .view_loader import ViewLoader


class Executor:
    def __init__(
        self,
        loader: ViewLoader,
        provenance: PairProvenance,
        budgets: Budgets | None = None,
    ):
        self.loader = loader
        self.provenance = provenance
        self.budgets = budgets or Budgets()
        self.neighbors = Neighbors(loader.facts, loader)

    def run(
        self,
        plan: Plan,
        after_id: str | int | None = None,
        result_cap: int | None = None,
    ) -> Result:
        validate(plan)
        if any(stage.max_depth > self.budgets.max_depth for stage in plan.stages):
            from .errors import fail

            fail("E_BUDGET", "plan depth exceeds the executor budget")
        context = ExecutionContext(self.loader, self.neighbors, self.budgets, after_id)
        state = execute(plan, context)
        cap = self.budgets.result_cap if result_cap is None else result_cap
        if cap < 1:
            from .errors import fail

            fail("E_LIMIT", "result_cap must be positive")
        if len(state.values) > cap:
            state.values = state.values[:cap]
            state.truncated = True
            marker = state.values[-1].get("id", state.values[-1].get("_key"))
            if marker is not None:
                state.cursor = str(marker)
        return Result(
            state.shape,
            state.view,
            tuple(state.values),
            state.scalar,
            state.truncated,
            state.partial,
            state.unknown,
            state.cursor,
            self.provenance,
        )

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
