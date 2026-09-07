import shutil
import sqlite3
from pathlib import Path


def schema12_pair(facts: Path, project: Path, root: Path) -> tuple[Path, Path]:
    target_facts, target_project = (
        root / "graph-facts.sqlite",
        root / "graph-project.sqlite",
    )
    shutil.copy2(facts, target_facts)
    shutil.copy2(project, target_project)
    run, save = (1 << 32) + 1, (1 << 32) + 2
    with sqlite3.connect(target_facts) as db:
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
            "'callees',NULL,'all','',NULL,NULL,NULL,NULL,0,'complete',NULL,NULL)"
        )
        db.execute("INSERT INTO callgraph_run_root VALUES(1,?,?)", (run, "c:@F@run#"))
        db.execute(
            "INSERT INTO callgraph_run_edge VALUES(1,?,?,?,?,?,?,?,?)",
            (run, save, 1, 0, 1, 120, 1, 0),
        )
    return target_facts, target_project
