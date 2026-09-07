import sqlite3
from typing import cast


def one(
    db: sqlite3.Connection, sql: str, values: tuple[object, ...]
) -> sqlite3.Row | None:
    return cast(sqlite3.Row | None, db.execute(sql, values).fetchone())


def many(
    db: sqlite3.Connection, sql: str, values: tuple[object, ...]
) -> list[sqlite3.Row]:
    return list(db.execute(sql, values))


def run(db: sqlite3.Connection, run_id: int) -> sqlite3.Row | None:
    return one(db, "SELECT * FROM callgraph_run WHERE run_id=?", (run_id,))


def roots(
    db: sqlite3.Connection, run_id: int, limit: int, offset: int = 0
) -> list[sqlite3.Row]:
    return many(
        db,
        "SELECT * FROM callgraph_run_root WHERE run_id=? "
        "ORDER BY symbol_id,usr LIMIT ? OFFSET ?",
        (run_id, limit, offset),
    )


def targets(
    db: sqlite3.Connection, run_id: int, limit: int, offset: int = 0
) -> list[sqlite3.Row]:
    return many(
        db,
        "SELECT * FROM callgraph_run_target WHERE run_id=? "
        "ORDER BY symbol_id,usr LIMIT ? OFFSET ?",
        (run_id, limit, offset),
    )


def edges(
    db: sqlite3.Connection, run_id: int, limit: int, offset: int = 0
) -> list[sqlite3.Row]:
    sql = (
        "SELECT e.*,s.usr AS source_usr,d.usr AS target_usr "
        "FROM callgraph_run_edge e JOIN symbol s ON s.id=e.source_id "
        "JOIN symbol d ON d.id=e.destination_id WHERE e.run_id=? "
        "ORDER BY e.depth,e.source_id,e.destination_id,e.kind,e.position "
        "LIMIT ? OFFSET ?"
    )
    return many(db, sql, (run_id, limit, offset))


def sites(db: sqlite3.Connection, edge: sqlite3.Row) -> list[sqlite3.Row]:
    sql = (
        "SELECT * FROM relation_site WHERE source_id=? AND destination_id=? "
        "AND kind=? AND position=? AND file_id=? AND offset=?"
    )
    return many(
        db,
        sql,
        tuple(
            edge[key]
            for key in (
                "source_id",
                "destination_id",
                "kind",
                "position",
                "file_id",
                "offset",
            )
        ),
    )


def frontier(
    db: sqlite3.Connection, run_id: int, limit: int, offset: int = 0
) -> list[sqlite3.Row]:
    sql = (
        "SELECT f.*,s.usr FROM callgraph_run_frontier f "
        "JOIN symbol s ON s.id=f.symbol_id WHERE f.run_id=? "
        "ORDER BY f.symbol_id,f.reason LIMIT ? OFFSET ?"
    )
    return many(db, sql, (run_id, limit, offset))


def recovery(
    db: sqlite3.Connection, run_id: int, limit: int, offset: int = 0
) -> list[sqlite3.Row]:
    return many(
        db,
        "SELECT * FROM callgraph_run_recovery WHERE run_id=? "
        "ORDER BY tu_file_id LIMIT ? OFFSET ?",
        (run_id, limit, offset),
    )
