import sqlite3


def add_run(facts):
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
        db.executemany(
            "INSERT INTO callgraph_run_target VALUES(1,?,?)",
            (
                (ids["persist"], "c:@F@persist#"),
                (ids["run"], "c:@F@run#"),
                (ids["save"], "c:@F@save#"),
            ),
        )
        db.executemany(
            "INSERT INTO callgraph_run_root VALUES(1,?,?)",
            (
                (ids["persist"], "c:@F@persist#"),
                (ids["save"], "c:@F@save#"),
            ),
        )
        db.executemany(
            "INSERT INTO callgraph_run_edge VALUES(1,?,?,?,?,?,?,?,?)",
            (
                (ids["run"], ids["save"], 1, 0, 1, 120, 1, 0),
                (ids["save"], ids["persist"], 1, 0, 1, 80, 2, 0),
                (ids["run"], ids["save"], 1, 0, 1, 130, 1, 0),
                (ids["run"], ids["save"], 1, 0, 1, 140, 1, 0),
            ),
        )
        db.executemany(
            "INSERT INTO callgraph_run_frontier VALUES(1,?,?)",
            (
                (ids["persist"], "max_depth"),
                (ids["run"], "budget"),
                (ids["save"], "cycle"),
            ),
        )
        db.executemany(
            "INSERT INTO callgraph_run_recovery VALUES(1,?,?,?)",
            (
                (1, "reused", "cached"),
                (2, "recovered", "replayed"),
                (3, "failed", "missing"),
            ),
        )
