import sqlite3


def count(db: sqlite3.Connection, table: str, run_id: int) -> int:
    row = db.execute(
        f"SELECT COUNT(*) FROM {table} WHERE run_id=?", (run_id,)
    ).fetchone()
    return int(row[0])


def path_flags(db: sqlite3.Connection, run_id: int) -> tuple[bool, bool, bool]:
    exists = (
        db.execute(
            "SELECT 1 FROM callgraph_run_target WHERE run_id=? LIMIT 1", (run_id,)
        ).fetchone()
        is not None
    )
    if not exists:
        return False, False, False
    reached = (
        db.execute(
            "SELECT 1 FROM callgraph_run_edge e JOIN callgraph_run_target t "
            "ON t.run_id=e.run_id AND t.symbol_id=e.destination_id "
            "WHERE e.run_id=? LIMIT 1",
            (run_id,),
        ).fetchone()
        is not None
    )
    self_path = (
        db.execute(
            "SELECT 1 FROM callgraph_run_root r JOIN callgraph_run_target t "
            "ON t.run_id=r.run_id AND t.symbol_id=r.symbol_id "
            "WHERE r.run_id=? LIMIT 1",
            (run_id,),
        ).fetchone()
        is not None
    )
    return True, reached, self_path
