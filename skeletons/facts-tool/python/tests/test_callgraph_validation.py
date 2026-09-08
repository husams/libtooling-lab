import pytest
from support.callgraph_seed import add_run

from facts_tool import FactsToolError, open_codebase


def test_legacy_graph_reader_is_explicitly_unavailable(paired_databases):
    with (
        open_codebase(
            facts_db=paired_databases[0], project_db=paired_databases[1]
        ) as cb,
        pytest.raises(FactsToolError, match="E_CAPABILITY"),
    ):
        cb.callgraphs.latest()


def test_run_cursor_rejects_bool(paired_databases):
    facts = paired_databases[0]
    add_run(facts)
    with (
        open_codebase(facts_db=facts, project_db=paired_databases[1]) as cb,
        pytest.raises(FactsToolError, match="E_LIMIT"),
    ):
        cb.callgraphs.list(after=True)


@pytest.mark.parametrize(
    ("status", "expected"),
    (
        ("complete", "unreachable"),
        ("truncated", "truncated"),
        ("cancelled", "cancelled"),
        ("recovery-failed", "recovery-failed"),
        ("failed", "failed"),
    ),
)
def test_run_outcome_matrix(paired_databases, status, expected):
    facts = paired_databases[0]
    add_run(facts)
    import sqlite3

    with sqlite3.connect(facts) as db:
        db.execute("UPDATE callgraph_run SET status=?", (status,))
        db.execute("DELETE FROM callgraph_run_root")
        db.execute("DELETE FROM callgraph_run_edge")
    with open_codebase(facts_db=facts, project_db=paired_databases[1]) as cb:
        assert cb.callgraphs.latest().path_outcome == expected
