RELATION_NAMES = (
    "calls",
    "inherits",
    "contains",
    "specializes",
    "instantiates",
    "overrides",
    "uses",
    "field_of",
    "method_of",
    "construct_value",
    "construct_temp",
    "construct_heap",
    "construct_copy",
    "construct_move",
    "factory_construct",
    "destroy",
    "friend",
    "dispatch_calls",
    "alias_of",
    "of_type",
    "return_type",
    "param_type",
    "template_argument_type",
)
RELATION_IDS = {name: index for index, name in enumerate(RELATION_NAMES, 1)}
PSEUDO_RELATIONS = {
    "has_parameter",
    "has_template_parameter",
    "has_template_argument",
    "includes",
    "definition",
    "declaration",
}
ALIASES = {
    "call": "calls",
    "base": "inherits",
    "member": "contains",
    "construct-value": "construct_value",
    "construct-temp": "construct_temp",
    "construct-heap": "construct_heap",
    "construct-copy": "construct_copy",
    "construct-move": "construct_move",
    "factory-construct": "factory_construct",
    "dispatch-calls": "dispatch_calls",
    "alias-of": "alias_of",
    "of-type": "of_type",
    "return-type": "return_type",
    "param-type": "param_type",
    "template-argument-type": "template_argument_type",
}


def relation_name(value: str) -> str:
    key = value.strip().lower().replace(" ", "_")
    return ALIASES.get(key, key)


def relation_id(value: str) -> int | None:
    return RELATION_IDS.get(relation_name(value))
