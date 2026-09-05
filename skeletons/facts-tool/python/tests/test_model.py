from pathlib import Path

from facts_tool import Callable, Record, open_codebase


def test_typed_navigation_uses_shared_executor(
    paired_databases: tuple[Path, Path],
) -> None:
    with open_codebase(
        facts_db=paired_databases[0], project_db=paired_databases[1]
    ) as cb:
        run = cb.get("app::run")
        box = cb.get("app::Box")
        assert isinstance(run, Callable)
        assert isinstance(box, Record)
        assert [callee.name for callee in run.callees(2)] == ["save", "persist"]
        assert [field.name for field in box.fields()] == ["value"]
        assert run.parameters()[0]["default_expression"] == "{}"


def test_fluent_query_lowers_to_plan(paired_databases: tuple[Path, Path]) -> None:
    with open_codebase(
        facts_db=paired_databases[0], project_db=paired_databases[1]
    ) as cb:
        query = cb.query("app::run").relation("calls", max_depth=2)
        assert query.to_plan() == query.plan
        assert query.names() == ["save", "persist"]
        assert query.filter(lambda row: row["name"] == "save").names() == ["save"]
