from pytest_bdd import then, when

from facts_tool.queryplan import (
    all_targets,
    any_target,
    called_by,
    calls,
    codebase,
    has_ancestor,
    has_field,
    has_member,
    has_method,
    has_nested,
    has_template_arg,
    implements,
    inherits_from,
    is_abstract,
    is_instance,
    is_instantiation_of,
    is_interface,
    is_pure,
    is_specialization_of,
    is_static,
    is_template,
    no_targets,
    nodes,
    start,
    used_by,
    uses,
)

from .matrix import run_matrix


@when("I execute every semantic helper and target-set constructor")
def semantic_helpers(cb, world):
    predicates = (
        inherits_from(any_target(("app::Box",))),
        inherits_from(all_targets(("app::Box",))),
        inherits_from(no_targets(("missing",))),
        implements("app::Box"),
        has_ancestor("app::Box"),
        has_member(),
        has_method(),
        has_field(),
        has_nested(),
        has_template_arg(),
        is_specialization_of("app::Box"),
        is_instantiation_of("app::Box"),
        calls(),
        called_by(),
        uses(),
        used_by(),
        is_abstract(),
        is_interface(),
        is_pure(),
        is_static(),
        is_template(),
        is_instance(),
    )
    world["helper_results"] = run_matrix(
        cb,
        lambda database, _: [
            database.executor.run((start(codebase()) | nodes(pred)).plan)
            for pred in predicates
        ],
    )


@then("every helper produces a valid executable predicate")
def helper_result(world):
    assert all(len(results) == 22 for _, results in world["helper_results"])
