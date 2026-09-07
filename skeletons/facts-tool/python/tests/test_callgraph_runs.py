import sqlite3

import pytest

from facts_tool import FactsToolError, open_codebase


def _add_run(facts):
    ids = {
        name: (1 << 32) + index
        for index, name in enumerate(("run", "save", "persist"), 1)
    }
    with sqlite3.connect(facts) as db:
        db.executescript("""
        CREATE TABLE callgraph_run(run_id INTEGER PRIMARY KEY,created_at TEXT,
          project_path TEXT,facts_path TEXT,mode TEXT,path_mode TEXT,
          calls_scope TEXT,components TEXT,max_depth INTEGER,max_nodes INTEGER,
          max_edges INTEGER,time_limit_ms INTEGER,recover_missing INTEGER,
          status TEXT,truncation_reason TEXT,error TEXT);
        CREATE TABLE callgraph_run_root(run_id INTEGER,symbol_id INTEGER,usr TEXT);
        CREATE TABLE callgraph_run_target(run_id INTEGER,symbol_id INTEGER,usr TEXT);
        CREATE TABLE callgraph_run_edge(run_id INTEGER,source_id INTEGER,
          destination_id INTEGER,kind INTEGER,position INTEGER,file_id INTEGER,
          offset INTEGER,depth INTEGER,cycle INTEGER);
        CREATE TABLE callgraph_run_frontier(
          run_id INTEGER,symbol_id INTEGER,reason TEXT);
        CREATE TABLE callgraph_run_recovery(run_id INTEGER,tu_file_id INTEGER,
          outcome TEXT,diagnostic TEXT);
        PRAGMA user_version=12;
        """)
        db.execute(
            "INSERT INTO callgraph_run VALUES(1,'now','project','facts',"
            "'path','shortest','all','',NULL,NULL,NULL,NULL,0,'complete',NULL,NULL)"
        )
        db.execute(
            "INSERT INTO callgraph_run_root VALUES(1,?,?)", (ids["run"], "c:@F@run#")
        )
        db.execute(
            "INSERT INTO callgraph_run_target VALUES(1,?,?)",
            (ids["persist"], "c:@F@persist#"),
        )
        db.execute(
            "INSERT INTO callgraph_run_root VALUES(1,?,?)",
            (ids["persist"], "c:@F@persist#"),
        )
        db.execute(
            "INSERT INTO callgraph_run_edge VALUES(1,?,?,?,?,?,?,?,?)",
            (ids["run"], ids["save"], 1, 0, 1, 120, 1, 0),
        )
        db.execute(
            "INSERT INTO callgraph_run_edge VALUES(1,?,?,?,?,?,?,?,?)",
            (ids["save"], ids["persist"], 1, 0, 1, 80, 2, 0),
        )
        db.execute(
            "INSERT INTO callgraph_run_frontier VALUES(1,?,?)",
            (ids["persist"], "max_depth"),
        )
        db.execute(
            "INSERT INTO callgraph_run_recovery VALUES(1,?,?,?)",
            (1, "reused", "cached"),
        )


def test_schema12_run_reader_is_bounded_and_read_only(paired_databases):
    facts, project = paired_databases
    _add_run(facts)
    before = (facts.read_bytes(), project.read_bytes())
    with open_codebase(facts_db=facts, project_db=project) as cb:
        assert [run.run_id for run in cb.callgraphs.list(limit=1)] == [1]
        run = cb.callgraphs.get(1, limit=1)
        assert run.status == "complete" and run.path_found
        assert run.path_outcome == "found" and run.edges.total == 2
        assert run.edges.next_cursor == 1 and run.roots.total == 2
        next_run = cb.callgraphs.get(1, limit=1, cursors={"edges": 1})
        assert next_run.edges[0].source.qualified_name == "app::save"
        empty_page = cb.callgraphs.get(1, limit=1, cursors={"edges": 2})
        assert not empty_page.edges and empty_page.edges.total == 2
        assert not next_run.edges[0].site.enriched
        assert run.edges[0].semantic_kind == "Calls"
        assert run.edges[0].site and run.edges[0].site.offset == 120
        assert run.frontier[0].reason == "max_depth"
        assert run.recovery[0].outcome == "reused"
    assert (facts.read_bytes(), project.read_bytes()) == before

