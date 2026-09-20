from unittest.mock import patch

from facts_tool import open_codebase
from facts_tool.queryplan import out, start, symbol
from facts_tool.view_symbols import _symbol


def test_graph_walk_hydrates_only_visited_symbols(paired_databases):
    facts, project = paired_databases
    with open_codebase(facts_db=facts, project_db=project) as cb:
        with patch("facts_tool.view_symbols._symbol", wraps=_symbol) as decode:
            query = start(symbol("app::run")) | out("calls", 1, 2)
            names = [row["name"] for row in cb.executor.run(query.plan)]
        assert names == ["save", "persist"]
        assert decode.call_count == 3


def test_inbound_owned_relation_hydrates_only_owners(paired_databases):
    facts, project = paired_databases
    with open_codebase(facts_db=facts, project_db=project) as cb:
        parameter = cb.executor.loader.load("parameter")[0]
        with patch("facts_tool.view_symbols._symbol", wraps=_symbol) as decode:
            owners = cb.executor.neighbors([parameter], "has_parameter", True)
        assert [row["name"] for row in owners] == ["run"]
        assert decode.call_count == 1
