from pytest_bdd import scenarios, then, when

from facts_tool.queryplan import codebase, nodes, out, start, symbol, view

scenarios("../features/typed-facts.feature")


@when("I query callable qualifiers and return facts")
def callable_facts(cb, world):
    run = cb.executor.run(start(symbol("app::run")).plan).nodes[0]
    target = cb.executor.run(
        (start(symbol("app::run")) | out("return_type")).plan
    ).nodes
    spelling = cb.executor.run(
        (start(symbol("app::run")) | out("definition")).plan
    ).nodes
    world["callable"] = run, target, spelling


@then("canonical spelling and return target remain distinct")
def callable_result(world):
    run, targets, definitions = world["callable"]
    assert run["return_type_spelling"] == "int" and run["is_noexcept"]
    assert [row["name"] for row in targets] == ["int"]
    assert definitions[0]["file"].endswith("save.cpp")


@when("I query enum enumerator and initializer views")
def enum_facts(cb, world):
    views = ("enumeration", "enumerator", "initializer")
    world["details"] = tuple(
        cb.executor.run((start(codebase()) | view(name) | nodes()).plan).nodes
        for name in views
    )


@then("written evaluated and underlying values are preserved")
def enum_result(world):
    enumeration, enumerator, initializer = world["details"]
    assert enumeration[0]["underlying_type"] == 1
    assert (enumerator[0]["value"], enumerator[0]["initializer_expression"]) == (
        "1",
        "1",
    )
    assert (initializer[0]["expression"], initializer[0]["evaluated_value"]) == (
        "42",
        "42",
    )
    assert enumeration[0]["is_scoped"] is True


@when("I query every declared slot and supplied template value")
def template_facts(cb, world):
    slots = start(symbol("app::Box")) | out("has_template_parameter")
    values = start(symbol("app::Box<int>")) | out("has_template_argument")
    world["template_facts"] = (
        cb.executor.run(slots.plan).nodes,
        cb.executor.run(values.plan).nodes,
    )


@then("order pack kind and flags use conventional names")
def template_results(world):
    slots, values = world["template_facts"]
    assert [(row["position"], row["name"]) for row in slots] == [(0, "T"), (1, "Args")]
    assert slots[1]["is_parameter_pack"] and slots[1]["is_non_type"]
    assert (values[1]["position"], values[1]["value"], values[1]["kind"]) == (1, "7", 1)
    assert values[1]["is_pack"] and values[1]["is_const"]
