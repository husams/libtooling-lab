from pathlib import Path

from facts_tool import open_codebase
from facts_tool.queryplan import (
    codebase,
    count,
    eq,
    exists,
    in_,
    intersect,
    nodes,
    order_by,
    out,
    path,
    rank,
    select,
    sites,
    start,
    symbol,
    view,
    where,
)


def test_predicates_traversal_and_shaping(paired_databases: tuple[Path, Path]) -> None:
    with open_codebase(
        facts_db=paired_databases[0], project_db=paired_databases[1]
    ) as cb:
        query = (
            start(codebase())
            | nodes(eq("kind", "function"))
            | where(exists("calls"))
            | select(("name",))
            | order_by(("name",))
        )
        result = cb.executor.run(query.plan)
    assert result.to_dict()["rows"] == [{"name": "run"}, {"name": "save"}]


def test_depth_set_algebra_and_count(paired_databases: tuple[Path, Path]) -> None:
    with open_codebase(
        facts_db=paired_databases[0], project_db=paired_databases[1]
    ) as cb:
        reachable = start(symbol("app::run")) | out("calls", 1, 2)
        shared = reachable | intersect(start(symbol("app::save")) | out("calls"))
        assert cb.executor.run((shared | count()).plan).scalar == 1
        assert {row["name"] for row in cb.executor.run(reachable.plan)} == {
            "save",
            "persist",
        }


def test_paths_sites_and_project_view(paired_databases: tuple[Path, Path]) -> None:
    with open_codebase(
        facts_db=paired_databases[0], project_db=paired_databases[1]
    ) as cb:
        witness = (
            start(symbol("app::run"))
            | path(start(symbol("app::persist")), "calls")
            | rank(1)
        )
        assert cb.executor.run(witness.plan).paths[0]["length"] == 2
        evidence = (
            start(codebase()) | view("edge") | nodes(eq("kind", "calls")) | sites()
        )
        assert len(cb.executor.run(evidence.plan)) == 3
        files = start(codebase()) | view("file") | nodes() | in_("includes")
        assert [row["name"] for row in cb.executor.run(files.plan)] == ["main.cpp"]


def test_typed_transition_and_empty_filter_do_not_reenumerate(
    paired_databases: tuple[Path, Path],
) -> None:
    with open_codebase(
        facts_db=paired_databases[0], project_db=paired_databases[1]
    ) as cb:
        parameter = (
            start(symbol("app::run"))
            | out("has_parameter")
            | where(eq("position", 0))
            | select(("name",))
        )
        empty = start(codebase()) | nodes(eq("name", "absent")) | nodes()
        assert cb.executor.run(parameter.plan).rows[0]["name"] == "box"
        assert not cb.executor.run(empty.plan).nodes
