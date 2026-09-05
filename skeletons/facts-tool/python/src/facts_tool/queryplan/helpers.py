from .predicates import any_of, eq, in_list
from .quantifiers import exists
from .targets import target_ref, target_set_pred
from .types import Pred, TargetSet


def _related(
    name: str, target: str | TargetSet, *, inbound: bool = False, depth: int = 1
) -> Pred:
    if isinstance(target, TargetSet):
        return target_set_pred(name, target, inbound, depth)
    return exists(name, target_ref(target), 1, depth, inbound)


def inherits_from(target: str | TargetSet, transitive: bool = False) -> Pred:
    return _related("inherits", target, depth=32 if transitive else 1)


def implements(target: str | TargetSet) -> Pred:
    return _related("inherits", target)


def has_ancestor(target: str, transitive: bool = True) -> Pred:
    return inherits_from(target, transitive)


def has_member(target: Pred | None = None) -> Pred:
    return any_of(
        (
            exists("field_of", target, inbound=True),
            exists("method_of", target, inbound=True),
        )
    )


def has_method(target: Pred | None = None) -> Pred:
    return exists("method_of", target, inbound=True)


def has_field(target: Pred | None = None) -> Pred:
    return exists("field_of", target, inbound=True)


def has_nested(target: Pred | None = None) -> Pred:
    return exists("contains", target)


def has_template_arg(target: Pred | None = None) -> Pred:
    return exists("has_template_argument", target)


def is_specialization_of(target: str) -> Pred:
    return exists("specializes", target_ref(target))


def is_instantiation_of(target: str) -> Pred:
    return exists("instantiates", target_ref(target))


def calls(target: Pred | None = None) -> Pred:
    return exists("calls", target)


def called_by(target: Pred | None = None) -> Pred:
    return exists("calls", target, inbound=True)


def uses(target: Pred | None = None) -> Pred:
    return exists("uses", target)


def used_by(target: Pred | None = None) -> Pred:
    return exists("uses", target, inbound=True)


def is_abstract() -> Pred:
    return eq("is_abstract", True)


def is_interface() -> Pred:
    return any_of((eq("kind", "protocol"), eq("kind", "interface")))


def is_pure() -> Pred:
    return eq("is_pure", True)


def is_static() -> Pred:
    return eq("is_static", True)


def is_template() -> Pred:
    return in_list("kind", ("class_template", "function_template"))


def is_instance() -> Pred:
    return exists("instantiates")
