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


@when("I apply union intersection and except")
def set_operations(cb, world):
    left = start(symbol("app::run")) | out("calls", 1, 2)
    right = start(symbol("app::save")) | out("calls")
    world["sets"] = tuple(
        len(cb.executor.run((left | stage(right)).plan))
        for stage in (union_, intersect, except_)
    )


@then("all set operation expectations hold")
def set_results(world):
    assert world["sets"] == (2, 1, 1)


@when("I select distinct ordered limited names and count")
def shaping(cb, world):
    base = start(symbol("app::run")) | out("calls", 1, 2)
    rows = base | select(("name",)) | distinct() | order_by(("name",)) | limit(1)
    world["shapes"] = (
        [row["name"] for row in cb.executor.run(rows.plan)],
        cb.executor.run((base | count()).plan).scalar,
    )


@then("all shaping expectations hold")
def shape_results(world):
    assert world["shapes"] == (["persist"], 2)


@when("I find a ranked call path from run to persist")
def ranked_path(cb, world):
    query = (
        start(symbol("app::run"))
        | path(start(symbol("app::persist")), "calls")
        | rank(1)
    )
    world["path"] = cb.executor.run(query.plan).paths[0]


@then("the path has two steps and source evidence")
def path_result(world):
    assert world["path"]["length"] == 2
    assert all(step["sites"] for step in world["path"]["steps"])


@when("I find direct users of int")
def reverse_types(cb, world):
    world["types"] = cb.executor.run((start(symbol("int")) | reverse_type_use()).plan)


@then("type-use witnesses are returned honestly")
def type_results(world):
    assert world["types"].shape == "path" and len(world["types"]) >= 3
    assert world["types"].partial
