from pytest_bdd import given, scenarios, then, when

scenarios("../features/callgraph.feature")


@given("a current native schema13 pair", target_fixture="cb")
def current_native_pair(native_current_codebase):
    return native_current_codebase


@when("I read its persisted graph and ordinary navigation")
def read_graph_and_navigation(cb, world):
    run = cb.callgraphs.latest()
    world["graph"] = (run.status, run.provenance.facts.schema.user_version)
    world["navigation"] = (
        [item.qualified_name for item in cb.find("app::run").callees()],
        [item.qualified_name for item in cb.find("app::save").callers()],
    )


@then("graph provenance and relation navigation are distinct")
def verify_graph_and_navigation(world):
    assert world["graph"] == ("complete", 13)
    assert world["navigation"] == (["app::save"], ["app::run"])
