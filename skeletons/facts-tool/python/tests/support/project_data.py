import sqlite3
from pathlib import Path


def add_project(db: sqlite3.Connection, root: Path) -> None:
    db.execute("INSERT INTO semantic_universe VALUES(1,'demo','Demo','explicit')")
    db.execute("INSERT INTO repository VALUES(1,'demo','repo',NULL,1,1)")
    db.execute("INSERT INTO clone VALUES(1,1,?,'active')", (str(root),))
    db.execute("INSERT INTO component VALUES(1,'app','src','repo',NULL,1,1)")
    db.execute("INSERT INTO directory VALUES(1,1,'.')")
    files = (
        (1, 1, "main.cpp", 1.0, "a", "-std=c++23", "clang++", str(root), 1, None, 0),
        (2, 1, "save.cpp", 1.0, "b", "-std=c++23", "clang++", str(root), 1, None, 0),
    )
    db.executemany("INSERT INTO file VALUES(?,?,?,?,?,?,?,?,?,?,?)", files)
    db.execute("INSERT INTO project_registry VALUES(1,1,'fixture',2)")
