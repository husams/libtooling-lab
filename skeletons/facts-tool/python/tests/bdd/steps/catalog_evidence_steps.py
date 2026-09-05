from pytest_bdd import then, when

from facts_tool.queryplan import codebase, eq, nodes, sites, start, view


@when("I query call relation sites")
def relation_sites(cb, world):
    edges = start(codebase()) | view("edge") | nodes(eq("kind", "calls"))
    world["edges"] = cb.executor.run(edges.plan).nodes
    world["sites"] = cb.executor.run((edges | sites()).plan).nodes


@then("repeated sites and full relation keys are preserved")
def site_result(world):
    assert len(world["sites"]) == 3
    assert len({row["id"] for row in world["sites"]}) == 3
    assert all(
        {"source_id", "destination_id", "position"} <= row.keys()
        for row in world["sites"]
    )
    rich = next(row for row in world["edges"] if row["access"] == "public")
    assert rich["count"] == 2 and rich["is_implicit"] and rich["is_lexical"]
    assert any(
        row["receiver_type_id"] and row["certainty"] == 1 for row in world["sites"]
    )
