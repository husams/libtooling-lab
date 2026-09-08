import sqlite3

from support.callgraph_seed import add_run

from facts_tool import open_codebase


def test_schema12_run_reader_is_bounded_and_read_only(paired_databases):
    facts, project = paired_databases
    add_run(facts)
    with sqlite3.connect(facts) as db:
        db.execute(
            "INSERT INTO callgraph_run VALUES(2,'later','project','facts',"
            "'path','shortest','all','',NULL,NULL,NULL,NULL,0,'complete',NULL,NULL)"
        )
    before = (facts.read_bytes(), project.read_bytes())
    with open_codebase(facts_db=facts, project_db=project) as cb:
        first = cb.callgraphs.list(limit=1)
        second = cb.callgraphs.list(limit=1, after=first[-1].run_id)
        assert [run.run_id for run in first] == [1]
        assert [run.run_id for run in second] == [2]
        assert second[0].roots.total == 0 and second[0].roots.complete
        run = cb.callgraphs.get(1, limit=1)
        assert run.status == "complete" and run.path_found
        assert run.target_reached and run.self_path
        assert run.path_outcome == "found" and run.edges.total == 4
        assert run.edges.next_cursor == 1 and run.edges.total == 4
        assert run.edges[0].site and run.edges[0].site.offset == 120
        next_run = cb.callgraphs.get(1, limit=1, cursors={"edges": 1})
        assert next_run.edges[0].site and next_run.edges[0].site.offset == 130
        last_page = cb.callgraphs.get(1, limit=1, cursors={"edges": 2})
        assert last_page.edges[0].site and last_page.edges[0].site.offset == 140
        fourth_page = cb.callgraphs.get(1, limit=1, cursors={"edges": 3})
        assert fourth_page.edges[0].site and fourth_page.edges[0].site.offset == 80
        empty_page = cb.callgraphs.get(1, limit=1, cursors={"edges": 4})
        assert not empty_page.edges and empty_page.edges.total == 4
        for name, key in (
            ("roots", lambda item: item.symbol.usr),
            ("targets", lambda item: item.symbol.usr),
            ("edges", lambda item: item.site.offset),
            ("frontier", lambda item: (item.symbol.usr, item.reason)),
            ("recovery", lambda item: item.tu_file_id),
        ):
            values, cursor = [], 0
            while True:
                page = cb.callgraphs.get(1, limit=1, cursors={name: cursor})
                collection = getattr(page, name)
                values.extend(key(item) for item in collection.items)
                if collection.next_cursor is None:
                    assert len(values) == collection.total
                    assert len(values) == len(set(values))
                    break
                cursor = collection.next_cursor
        assert not next_run.edges[0].site.enriched
        assert run.edges[0].semantic_kind == "Calls"
        assert run.edges[0].site and run.edges[0].site.offset == 120
        assert run.frontier[0].reason == "budget"
        assert run.recovery[0].outcome == "reused"
    assert (facts.read_bytes(), project.read_bytes()) == before
