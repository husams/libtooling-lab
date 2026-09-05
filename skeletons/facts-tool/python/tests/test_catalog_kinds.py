from facts_tool.catalog_kinds import SYMBOL_KINDS, symbol_kind


def test_llvm_22_symbol_kind_values_are_exact() -> None:
    expected = {
        5: "include_directive",
        6: "enum",
        7: "struct",
        8: "class",
        13: "function",
        14: "variable",
        15: "field",
        16: "enum_constant",
        17: "instance_method",
        23: "constructor",
        24: "destructor",
        28: "template_type_parm",
        29: "template_template_parm",
        30: "non_type_template_parm",
        31: "concept",
    }
    assert len(SYMBOL_KINDS) == 32
    assert {value: symbol_kind(value) for value in expected} == expected
