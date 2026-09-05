from pytest_bdd import scenarios, then, when

from facts_tool.queryplan import codebase, nodes, out, start, symbol, view

from .matrix import run_matrix

scenarios("../features/typed-facts.feature")


@when("I query callable qualifiers and return facts")
def callable_facts(cb, world):
    def query(database, native):
        run = database.executor.run(start(symbol("app::run")).plan).nodes[0]
        target = database.executor.run(
            (start(symbol("app::run")) | out("return_type")).plan
        ).nodes
        definitions = database.executor.run(
            (start(symbol("app::run")) | out("definition")).plan
        ).nodes
        return run, target, definitions

    world["callable"] = run_matrix(cb, query)


@then("canonical spelling and return target remain distinct")
def callable_result(world):
    for native, (run, targets, definitions) in world["callable"]:
        assert run["return_type_spelling"] == "int" and run["is_noexcept"]
        assert [row["name"] for row in targets] == ["int"]
        assert definitions[0]["file"].endswith("source.cpp" if native else "save.cpp")


@when("I query enum enumerator and initializer views")
def enum_facts(cb, world):
    views = ("enumeration", "enumerator", "initializer")
    world["details"] = run_matrix(
        cb,
        lambda database, _: tuple(
            database.executor.run((start(codebase()) | view(name) | nodes()).plan).nodes
            for name in views
        ),
    )


@then("written evaluated and underlying values are preserved")
def enum_result(world):
    for _, (enumeration, enumerator, initializer) in world["details"]:
        assert enumeration[0]["underlying_type"] > 0
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
    def query(database, native):
        slots = start(symbol("app::Box")) | out("has_template_parameter")
        instance = "c:@N@app@S@Box>#I#p1VI7" if native else "app::Box<int>"
        values = start(symbol(instance)) | out("has_template_argument")
        return database.executor.run(slots.plan).nodes, database.executor.run(
            values.plan
        ).nodes

    world["template_facts"] = run_matrix(cb, query)


@then("order pack kind and flags use conventional names")
def template_results(world):
    for _, (slots, values) in world["template_facts"]:
        assert [(row["position"], row["name"]) for row in slots] == [
            (0, "T"),
            (1, "Args"),
        ]
        assert slots[1]["is_parameter_pack"] and slots[1]["is_non_type"]
        assert (values[1]["position"], values[1]["value"]) == (1, "7")
        assert values[1]["is_pack"] and values[1]["kind"] > 0
