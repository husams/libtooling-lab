from pytest_bdd import then, when

from facts_tool.queryplan import (
    all_of,
    any_of,
    codebase,
    eq,
    glob,
    in_list,
    ne,
    nodes,
    not_,
    order_by,
    select,
    start,
)

from .matrix import matrix_values, run_matrix


def names(database, query):
    return [row["name"] for row in database.executor.run(query.plan)]


@when("I select defined function names")
def select_functions(cb, world):
    pred = all_of(
        (
            eq("kind", "function"),
            eq("is_definition", True),
            in_list("name", ("run", "save")),
        )
    )
    query = start(codebase()) | nodes(pred) | select(("name",)) | order_by(("name",))
    world["names"] = run_matrix(cb, lambda database, _: names(database, query))


@when("I filter with every comparison and boolean constructor")
def comparisons(cb, world):
    pred = all_of(
        (
            in_list("kind", ("function",)),
            ne("name", "persist"),
            any_of((glob("name", "r*"), eq("name", "save"))),
            not_(eq("is_external", True)),
        )
    )
    query = start(codebase()) | nodes(pred) | select(("name",)) | order_by(("name",))
    world["names"] = run_matrix(cb, lambda database, _: names(database, query))


@then('the names are "run,save"')
def names_run_save(world):
    assert matrix_values(world["names"]) == [["run", "save"]] * 2


@then('the names are "save,persist"')
def names_save_persist(world):
    assert matrix_values(world["names"]) == [["save", "persist"]] * 2
