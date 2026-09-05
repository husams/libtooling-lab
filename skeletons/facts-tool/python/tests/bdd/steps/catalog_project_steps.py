from pytest_bdd import then, when

from facts_tool.queryplan import codebase, eq, nodes, out, start, symbol, view

from .matrix import run_matrix


@when("I query project files and include edges")
def project_files(cb, world):
    def query(database, native):
        source = "source.cpp" if native else "main.cpp"
        files = start(codebase()) | view("file") | nodes()
        include = start(codebase()) | view("file") | nodes(eq("name", source))
        return database.executor.run(files.plan).nodes, database.executor.run(
            (include | out("includes")).plan
        ).nodes

    world["project"] = run_matrix(cb, query)


@then("both project metadata and facts include data are used")
def project_result(world):
    for native, (files, included) in world["project"]:
        if native:
            assert any(str(row["driver"]).endswith("clang++") for row in files)
            assert [row["name"] for row in included] == ["api.hpp"]
        else:
            assert all(row["driver"] == "clang++" for row in files)
            assert [row["name"] for row in included] == ["save.cpp"]


@when("I explain a call query")
def explain_query(cb, world):
    query = (start(symbol("app::run")) | out("calls")).plan
    world["explain"] = run_matrix(
        cb, lambda database, _: database.executor.explain(query)
    )


@then("both database identities budgets and catalogs are reported")
def explain_result(world):
    for _, value in world["explain"]:
        assert value["provenance"]["facts"]["schema"]["user_version"] == 10
        assert value["budgets"]["max_depth"] == 32
        assert len(value["relations"]) == 23 and "file" in value["views"]
