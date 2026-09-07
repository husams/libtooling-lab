"""Child rows of one persisted `analyse call-graph` run."""
import sqlite3


def query(facts, sql, parameters=()):
    with sqlite3.connect(facts) as connection:
        return connection.execute(sql, parameters).fetchall()


def roots(facts, run_id):
    return query(facts, "SELECT s.qualified_name,r.usr FROM callgraph_run_root r "
                 "JOIN symbol s ON s.id=r.symbol_id WHERE r.run_id=? "
                 "ORDER BY s.qualified_name,r.usr", (run_id,))


def target(facts, run_id):
    rows = query(facts, "SELECT s.qualified_name,t.usr FROM callgraph_run_target "
                 "t JOIN symbol s ON s.id=t.symbol_id WHERE t.run_id=?", (run_id,))
    return rows[0] if rows else None


def edges(facts, run_id):
    """Reached edges as dicts with resolved source/target qualified names."""
    keys = ("source", "target", "kind", "position", "file_id", "offset", "depth",
            "cycle", "source_id", "target_id")
    rows = query(facts, "SELECT s.qualified_name,d.qualified_name,e.kind,"
                 "e.position,e.file_id,e.offset,e.depth,e.cycle,e.source_id,"
                 "e.destination_id FROM callgraph_run_edge e JOIN symbol s ON "
                 "s.id=e.source_id JOIN symbol d ON d.id=e.destination_id "
                 "WHERE e.run_id=? ORDER BY e.depth,s.qualified_name,"
                 "d.qualified_name,e.kind,e.position,e.file_id,e.offset",
                 (run_id,))
    return [dict(zip(keys, row)) for row in rows]


def edge_names(facts, run_id):
    return {(edge["source"], edge["target"]) for edge in edges(facts, run_id)}


def frontier(facts, run_id):
    return query(facts, "SELECT s.qualified_name,f.reason FROM "
                 "callgraph_run_frontier f JOIN symbol s ON s.id=f.symbol_id "
                 "WHERE f.run_id=? ORDER BY s.qualified_name,f.reason", (run_id,))


def recovery(facts, run_id):
    return query(facts, "SELECT tu_file_id,outcome,diagnostic FROM "
                 "callgraph_run_recovery WHERE run_id=? ORDER BY tu_file_id",
                 (run_id,))


def child_row_count(facts, run_id):
    return sum(query(facts, f"SELECT COUNT(*) FROM {table} WHERE run_id=?",
                     (run_id,))[0][0]
               for table in ("callgraph_run_root", "callgraph_run_target",
                             "callgraph_run_edge", "callgraph_run_frontier",
                             "callgraph_run_recovery"))
