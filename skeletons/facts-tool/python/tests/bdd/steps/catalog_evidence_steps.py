from pytest_bdd import then, when

from facts_tool.queryplan import codebase, eq, nodes, sites, start, view

from .matrix import run_matrix


@when("I query call relation sites")
def relation_sites(cb, world):
    edges = start(codebase()) | view("edge") | nodes(eq("kind", "calls"))
    world["evidence"] = run_matrix(
        cb,
        lambda database, _: (
            database.executor.run(edges.plan).nodes,
            database.executor.run((edges | sites()).plan).nodes,
        ),
    )


@then("repeated sites and full relation keys are preserved")
def site_result(world):
    for _, (edges, sites_rows) in world["evidence"]:
        assert len(sites_rows) >= 3
        assert len({row["id"] for row in sites_rows}) == len(sites_rows)
        assert all(
            {"source_id", "destination_id", "position"} <= row.keys()
            for row in sites_rows
        )
        rich = next(row for row in edges if row["count"] == 2)
        assert {"access", "is_implicit", "is_lexical", "is_virtual_base"} <= rich.keys()
        assert any(row["receiver_type_id"] or row["certainty"] for row in sites_rows)
