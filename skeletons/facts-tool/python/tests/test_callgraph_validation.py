import pytest

from facts_tool import FactsToolError, open_codebase
from test_callgraph_runs import _add_run


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
    _add_run(facts)
    with (
        open_codebase(facts_db=facts, project_db=paired_databases[1]) as cb,
        pytest.raises(FactsToolError, match="E_LIMIT"),
    ):
        cb.callgraphs.list(after=True)
