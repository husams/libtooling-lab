COLUMNS = (
    "id",
    "node",
    "kind",
    "sub_kind",
    "lang",
    "properties",
    "usr",
    "qualified_name",
    "line",
    "col",
    "offset",
    "access",
    "is_definition",
    "is_implicit",
    "is_static",
    "is_virtual",
    "is_const",
    "is_inline",
    "is_pure",
    "ref_qualifier",
    "is_override",
    "has_internal_linkage",
    "is_external",
    "is_variadic",
    "is_deleted",
    "is_defaulted",
    "is_explicit",
    "is_final",
    "is_abstract",
    "is_polymorphic",
    "has_extern_storage",
    "constant_evaluation",
    "is_noexcept",
    "is_volatile",
)


def symbol(
    symbol_id: int, node: int, kind: int, usr: str, name: str, **flags: object
) -> tuple[object, ...]:
    values: dict[str, object] = {
        "id": symbol_id,
        "node": node,
        "kind": kind,
        "sub_kind": 0,
        "lang": 2,
        "properties": 0,
        "usr": usr,
        "qualified_name": name,
        "line": 10,
        "col": 3,
        "offset": symbol_id & 0xFFFF,
        "access": "none",
        "ref_qualifier": "none",
        "constant_evaluation": "none",
    }
    flags_columns = (column for column in COLUMNS if column.startswith(("is_", "has_")))
    values.update({column: 0 for column in flags_columns})
    values.update(flags)
    return tuple(values[column] for column in COLUMNS)
