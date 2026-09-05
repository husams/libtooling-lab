from .errors import fail
from .execution_context import ExecutionContext
from .executor_nodes import apply_nodes, apply_traversal, apply_view, apply_where
from .executor_shapes import apply_shape
from .executor_special import apply_path, apply_reverse_type_use, apply_set, apply_sites
from .queryplan.types import Plan, Stage
from .source_exec import resolve_source
from .state import ExecutionState


def execute(plan: Plan, context: ExecutionContext) -> ExecutionState:
    state = resolve_source(plan.source, context.loader)
    for stage in plan.stages:
        _apply(state, stage, context)
    return state


def _operand(stage: Stage) -> Plan:
    if stage.operand is None:
        fail("E_STAGE", f"{stage.op} requires an operand")
    return stage.operand


def _child(context: ExecutionContext) -> ExecutionContext:
    return ExecutionContext(context.loader, context.neighbors, context.budgets)


def _apply(state: ExecutionState, stage: Stage, context: ExecutionContext) -> None:
    if stage.op == "view":
        apply_view(state, stage, context)
    elif stage.op == "nodes":
        apply_nodes(state, stage, context)
    elif stage.op == "where":
        apply_where(state, stage, context)
    elif stage.op in {"out", "in"}:
        apply_traversal(state, stage, context)
    elif stage.op == "sites":
        apply_sites(state, context)
    elif stage.op in {"union", "intersect", "except"}:
        apply_set(state, execute(_operand(stage), _child(context)), stage.op)
    elif stage.op == "path":
        apply_path(state, execute(_operand(stage), _child(context)), stage, context)
    elif stage.op == "reverse_type_use":
        apply_reverse_type_use(state, stage, context)
    elif not apply_shape(state, stage, context):
        fail("E_STAGE", f"unsupported stage {stage.op!r}")
