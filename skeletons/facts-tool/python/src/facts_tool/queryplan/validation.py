from facts_tool.catalog_views import VIEWS
from facts_tool.errors import fail
from facts_tool.fields import FIELDS

from .types import Plan
from .validate_predicates import depth, predicate, relation, target_view

STAGES = {
    "nodes",
    "view",
    "where",
    "out",
    "in",
    "sites",
    "union",
    "intersect",
    "except",
    "select",
    "count",
    "distinct",
    "order_by",
    "limit",
    "path",
    "rank",
    "reverse_type_use",
}


def validate(plan: Plan) -> None:
    if plan.source.kind not in {"codebase", "symbol", "entity"}:
        fail("E_SOURCE", f"unknown source {plan.source.kind!r}")
    shape, current_view = "nodes", "symbol"
    for stage in plan.stages:
        if stage.op not in STAGES:
            fail("E_STAGE", f"unknown stage {stage.op!r}")
        if stage.op in {"out", "in", "path"}:
            relation_name = relation(stage.relation)
            depth(stage.min_depth, stage.max_depth)
            if stage.op in {"out", "in"}:
                current_view = target_view(relation_name, stage.op == "in")
        if stage.op == "out" and stage.mode not in {"static", "devirtualized"}:
            fail("E_KIND", f"unknown traversal mode {stage.mode!r}")
        if stage.op == "out" and stage.mode == "devirtualized":
            fail(
                "E_CAPABILITY",
                "devirtualized traversal is unavailable; use dispatch_calls",
            )
        if stage.op == "sites" and current_view != "edge":
            fail("E_STAGE", "sites requires the edge view")
        if stage.op == "sites":
            current_view = "site"
        if stage.op == "reverse_type_use":
            depth(1, stage.max_depth)
            if current_view != "symbol":
                fail("E_STAGE", "reverse_type_use requires the symbol view")
        if stage.op == "view":
            if current_view == "site" and stage.level == "site":
                fail("E_STAGE", "view('site') cannot follow sites()")
            if stage.level in {"entity", "type", "type_layer", "call_argument"}:
                fail("E_CAPABILITY", f"view {stage.level!r} is not persisted")
            if stage.level not in VIEWS:
                fail("E_VIEW", f"unknown view {stage.level!r}")
            current_view = stage.level
        if stage.unknown not in {"exclude", "include", "error"}:
            fail("E_UNKNOWN", f"unknown policy {stage.unknown!r}")
        predicate(stage.pred, current_view)
        if stage.op in {"union", "intersect", "except", "path"}:
            if stage.operand is None:
                fail("E_STAGE", f"{stage.op} requires an operand")
            validate(stage.operand)
        _shape_transition(shape, stage.op)
        if stage.op in {"select", "order_by"}:
            bad = [
                field
                for field in stage.fields
                if field not in FIELDS.get(current_view, set())
            ]
            if bad:
                fail("E_FIELD", f"unknown {current_view} field(s): {', '.join(bad)}")
        if stage.op in {"limit", "rank"} and stage.n < 0:
            fail("E_LIMIT", f"{stage.op} value cannot be negative")
        if stage.op == "select":
            shape = "rows"
        elif stage.op == "count":
            shape = "scalar"
        elif stage.op in {"path", "reverse_type_use"}:
            shape = "path"


def _shape_transition(shape: str, operation: str) -> None:
    if shape == "scalar":
        fail("E_STAGE", f"{operation} cannot follow count")
    allowed = {"distinct", "order_by", "limit", "count"}
    if shape == "rows" and operation not in allowed:
        fail("E_STAGE", f"{operation} cannot follow select")
    if shape == "path" and operation not in {"rank", "distinct", "limit", "count"}:
        fail("E_STAGE", f"{operation} cannot follow path")
    if operation == "rank" and shape != "path":
        fail("E_STAGE", "rank requires a path result")
