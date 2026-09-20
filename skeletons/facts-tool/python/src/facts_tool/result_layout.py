from .queryplan.types import Plan
from .queryplan.validate_predicates import target_view


def result_layout(plan: Plan) -> tuple[str, str]:
    shape, view = "nodes", "symbol"
    for stage in plan.stages:
        if stage.op == "view":
            view = stage.level
        elif stage.op in {"out", "in"}:
            view = target_view(stage.relation, stage.op == "in")
        elif stage.op == "sites":
            view = "site"
        elif stage.op == "select":
            shape = "rows"
        elif stage.op == "count":
            shape = "scalar"
        elif stage.op in {"path", "reverse_type_use"}:
            shape, view = "path", "path"
    return shape, view
