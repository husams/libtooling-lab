from facts_tool.catalog_relations import PSEUDO_RELATIONS, relation_id, relation_name
from facts_tool.errors import fail
from facts_tool.fields import FIELDS

from .types import Pred


def depth(low: int, high: int) -> None:
    if low < 1 or high < low or high > 32:
        fail("E_DEPTH", f"invalid depth window {low}..{high}; maximum is 32")


def relation(value: str) -> str:
    name = relation_name(value)
    if relation_id(name) is None and name not in PSEUDO_RELATIONS:
        fail("E_RELATION", f"unknown relation {value!r}")
    return name


def target_view(name: str, inbound: bool) -> str:
    if name == "declaration":
        return "definition" if inbound else "symbol"
    if inbound and name in {
        "has_parameter",
        "has_template_parameter",
        "has_template_argument",
        "definition",
    }:
        return "symbol"
    return {
        "has_parameter": "parameter",
        "has_template_parameter": "template_parameter",
        "has_template_argument": "template_argument",
        "includes": "file",
        "definition": "definition",
        "declaration": "symbol",
    }.get(name, "symbol")


def predicate(pred: Pred | None, view: str) -> None:
    if pred is None:
        return
    valid = {
        "all_of",
        "any_of",
        "not",
        "eq",
        "ne",
        "glob",
        "in",
        "exists",
        "none",
        "all",
        "at_least",
        "exactly",
    }
    if pred.op not in valid:
        fail("E_STAGE", f"unknown predicate operation {pred.op!r}")
    if pred.op == "not" and len(pred.kids) != 1:
        fail("E_STAGE", "not requires exactly one predicate")
    if pred.field and pred.field not in FIELDS.get(view, set()):
        fail("E_FIELD", f"unknown {view} field {pred.field!r}")
    for child in pred.kids:
        predicate(child, view)
    if pred.relation:
        name = relation(pred.relation)
        depth(pred.min_depth, pred.max_depth)
        predicate(pred.target, target_view(name, pred.inbound))
