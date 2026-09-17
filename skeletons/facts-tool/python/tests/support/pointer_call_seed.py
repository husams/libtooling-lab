import sqlite3


def add_pointer_run(facts):
    source, target = (1 << 32) | 1, (1 << 32) | 5
    with sqlite3.connect(facts) as db:
        db.executescript("""
        CREATE TABLE callgraph_pointer_call_site(source_id INTEGER,target_id INTEGER,
          file_id INTEGER,offset INTEGER,line INTEGER,col INTEGER,
          signature TEXT,expression TEXT);
        CREATE TABLE callgraph_run_pointer_call_site(run_id INTEGER,
          source_id INTEGER,target_id INTEGER,file_id INTEGER,offset INTEGER,
          line INTEGER,col INTEGER,signature TEXT,expression TEXT,
          target_name TEXT,target_usr TEXT);
        PRAGMA user_version=14;
        INSERT INTO callgraph_run VALUES(1,'now','project','facts',
          'callees',NULL,'all','',NULL,NULL,NULL,NULL,0,'complete',NULL,NULL);
        """)
        db.execute(
            "INSERT INTO callgraph_run_root VALUES(1,?,'c:@F@run#')", (source,)
        )
        db.executemany(
            "INSERT INTO callgraph_run_pointer_call_site VALUES(1,?,?,?,?,?,?,?,?,?,?)",
            (
                (source, target, 1, 200, 20, 3, "void (*)(int)", "fp", "fp", "fp-usr"),
                (source, None, 1, 240, 24, 3, "void (*)()", "factory()", None, None),
            ),
        )
    return source, target
