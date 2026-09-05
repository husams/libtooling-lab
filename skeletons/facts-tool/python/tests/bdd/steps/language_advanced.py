from pytest_bdd import then, when

from facts_tool.queryplan import (
    count,
    distinct,
    except_,
    intersect,
    limit,
    order_by,
    out,
    path,
    rank,
    reverse_type_use,
    select,
    start,
    symbol,
    union_,
)

from .matrix import matrix_values, run_matrix


@when("I apply union intersection and except")
def set_operations(cb, world):
    left = start(symbol("app::run")) | out("calls", 1, 2)
    right = start(symbol("app::save")) | out("calls")
    world["sets"] = run_matrix(
        cb,
        lambda database, _: tuple(
            len(database.executor.run((left | stage(right)).plan))
            for stage in (union_, intersect, except_)
        ),
    )


@then("all set operation expectations hold")
def set_results(world):
    assert matrix_values(world["sets"]) == [(2, 1, 1)] * 2


@when("I select distinct ordered limited names and count")
def shaping(cb, world):
    base = start(symbol("app::run")) | out("calls", 1, 2)
    rows = base | select(("name",)) | distinct() | order_by(("name",)) | limit(1)
    world["shapes"] = run_matrix(
        cb,
        lambda database, _: (
            [row["name"] for row in database.executor.run(rows.plan)],
            database.executor.run((base | count()).plan).scalar,
        ),
    )


@then("all shaping expectations hold")
def shape_results(world):
    assert matrix_values(world["shapes"]) == [(["persist"], 2)] * 2


@when("I find a ranked call path from run to persist")
def ranked_path(cb, world):
    query = (
        start(symbol("app::run"))
        | path(start(symbol("app::persist")), "calls")
        | rank(1)
    )
    world["path"] = run_matrix(
        cb, lambda database, _: database.executor.run(query.plan).paths[0]
    )


@then("the path has two steps and source evidence")
def path_result(world):
    for _, witness in world["path"]:
        assert witness["length"] == 2
        assert all(step["sites"] for step in witness["steps"])


@when("I find direct users of int")
def reverse_types(cb, world):
    query = (start(symbol("int")) | reverse_type_use()).plan
    world["types"] = run_matrix(cb, lambda database, _: database.executor.run(query))


@then("type-use witnesses are returned honestly")
def type_results(world):
    for _, result in world["types"]:
        assert result.shape == "path" and len(result) >= 3
        assert result.partial
