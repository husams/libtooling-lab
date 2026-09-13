import sqlite3

from .variableflow_models import Boundary, Edge, Location, Node, VariableFlowRun


def decode_run(db: sqlite3.Connection, row: sqlite3.Row) -> VariableFlowRun:
    run_id = int(row["run_id"])
    nodes = tuple(
        decode_node(item)
        for item in db.execute(
            "SELECT * FROM variable_flow_node WHERE run_id=? ORDER BY node_id",
            (run_id,),
        )
    )
    edges = tuple(
        Edge(
            int(item["source"]),
            int(item["target"]),
            str(item["kind"]),
            int(item["callsite"]),
        )
        for item in db.execute(
            "SELECT * FROM variable_flow_edge WHERE run_id=? "
            "ORDER BY source,target,kind,callsite",
            (run_id,),
        )
    )
    boundaries = tuple(
        Boundary(
            int(item["node"]),
            str(item["reason"]),
            str(item["detail"]),
            int(item["depth"]),
        )
        for item in db.execute(
            "SELECT * FROM variable_flow_boundary WHERE run_id=? "
            "ORDER BY node,depth,reason",
            (run_id,),
        )
    )
    sources = tuple(filter(None, str(row["sources"]).split("\n")))
    return VariableFlowRun(
        run_id,
        str(row["created_at"]),
        str(row["project_path"]),
        str(row["facts_path"]),
        str(row["function_selector"]),
        str(row["variable_selector"]),
        sources,
        row["declaration_line"],
        row["max_depth"],
        str(row["engine"]),
        str(row["assumptions"]),
        str(row["root_function"]),
        str(row["root_variable"]),
        str(row["status"]),
        nodes,
        edges,
        boundaries,
    )


def decode_node(row: sqlite3.Row) -> Node:
    location = Location(
        str(row["file"]),
        int(row["line"]),
        int(row["column_no"]),
        int(row["offset"]),
    )
    return Node(
        int(row["node_id"]),
        str(row["kind"]),
        str(row["function_usr"]),
        str(row["variable_usr"]),
        str(row["name"]),
        str(row["type"]),
        location,
        int(row["block"]),
        int(row["depth"]),
    )
