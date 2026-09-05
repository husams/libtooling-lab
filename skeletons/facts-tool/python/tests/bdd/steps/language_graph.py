from pytest_bdd import then, when

from facts_tool.queryplan import out, start, symbol

from .matrix import run_matrix


@when("I traverse diamond and cyclic call graphs")
def traverse_graph_shapes(cb, world):
    def query(database, native):
        prefix = "app::" if native else "fixture::"
        diamond = start(symbol(prefix + "diamond_source")) | out("calls", 1, 2)
        cycle = start(symbol(prefix + "cycle_a")) | out("calls", 1, 4)
        return (
            [row["id"] for row in database.executor.run(diamond.plan)],
            [row["id"] for row in database.executor.run(cycle.plan)],
        )

    world["graph_shapes"] = run_matrix(cb, query)


@then("each reachable identity appears once")
def graph_shape_results(world):
    assert all(
        len(ids) == len(set(ids))
        for _, shapes in world["graph_shapes"]
        for ids in shapes
    )
