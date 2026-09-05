SYMBOL_KINDS = (
    "unknown",
    "module",
    "namespace",
    "namespace_alias",
    "macro",
    "enum",
    "struct",
    "class",
    "protocol",
    "extension",
    "union",
    "type_alias",
    "function",
    "variable",
    "field",
    "enum_constant",
    "instance_method",
    "class_method",
    "static_method",
    "instance_property",
    "class_property",
    "static_property",
    "constructor",
    "destructor",
    "conversion_function",
    "parameter",
    "using",
    "concept",
    "comment_tag",
)
NODE_KINDS = {
    1: "function",
    2: "record",
    3: "enumeration",
    4: "variable",
    5: "symbol",
    6: "enumerator",
}


def symbol_kind(value: int) -> str:
    return SYMBOL_KINDS[value] if 0 <= value < len(SYMBOL_KINDS) else f"kind_{value}"
