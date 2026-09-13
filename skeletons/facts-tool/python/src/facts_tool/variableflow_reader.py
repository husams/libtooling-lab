import os
import sqlite3
from pathlib import Path

from .errors import fail
from .variableflow_models import Boundary, Edge, Location, Node, VariableFlowRun
from .variableflow_schema import require_schema


class VariableFlowReader:
    def __init__(self, path: str | os.PathLike[str]):
        self.path = os.fspath(path)
        try:
            self._db = sqlite3.connect(
                Path(self.path).resolve().as_uri() + "?mode=ro", uri=True
            )
        except sqlite3.Error as error:
            fail("E_SOURCE", f"cannot open variable-flow database: {error}")
        self._db.row_factory = sqlite3.Row
        try:
            require_schema(self._db)
        except Exception:
            self._db.close()
            raise
        self._closed = False

    def __enter__(self) -> "VariableFlowReader":
        return self

    def __exit__(self, *_args: object) -> None:
        self.close()

    def close(self) -> None:
        if not self._closed:
            self._db.close()
            self._closed = True

    def runs(self) -> tuple[VariableFlowRun, ...]:
        return tuple(self._decode(row) for row in self._db.execute(
            "SELECT * FROM variable_flow_run ORDER BY run_id"
        ))

    def get(self, run_id: int) -> VariableFlowRun:
        if not isinstance(run_id, int) or isinstance(run_id, bool) or run_id < 1:
            fail("E_SOURCE", "run_id must be a positive integer")
        row = self._db.execute(
            "SELECT * FROM variable_flow_run WHERE run_id=?", (run_id,)
        ).fetchone()
        if row is None:
            fail("E_SOURCE", f"variable flow run {run_id} not found")
        return self._decode(row)

    def _decode(self, row: sqlite3.Row) -> VariableFlowRun:
        run_id = int(row["run_id"])
        nodes = tuple(self._node(item) for item in self._db.execute(
            "SELECT * FROM variable_flow_node WHERE run_id=? ORDER BY node_id",
            (run_id,),
        ))
        edges = tuple(Edge(int(item["source"]), int(item["target"]),
                           str(item["kind"]), int(item["callsite"]))
                      for item in self._db.execute(
                          "SELECT * FROM variable_flow_edge WHERE run_id=? "
                          "ORDER BY source,target,kind,callsite", (run_id,)))
        boundaries = tuple(Boundary(int(item["node"]), str(item["reason"]),
                                    str(item["detail"]), int(item["depth"]))
                           for item in self._db.execute(
                               "SELECT * FROM variable_flow_boundary WHERE run_id=? "
                               "ORDER BY node,depth,reason", (run_id,)))
        sources = tuple(filter(None, str(row["sources"]).split("\n")))
        return VariableFlowRun(
            run_id, str(row["created_at"]), str(row["project_path"]),
            str(row["facts_path"]), str(row["function_selector"]),
            str(row["variable_selector"]), sources,
            row["declaration_line"], row["max_depth"], str(row["engine"]),
            str(row["assumptions"]), str(row["root_function"]),
            str(row["root_variable"]), str(row["status"]), nodes, edges,
            boundaries)

    @staticmethod
    def _node(row: sqlite3.Row) -> Node:
        location = Location(str(row["file"]), int(row["line"]),
                            int(row["column_no"]), int(row["offset"]))
        return Node(int(row["node_id"]), str(row["kind"]),
                    str(row["function_usr"]), str(row["variable_usr"]),
                    str(row["name"]), str(row["type"]), location,
                    int(row["block"]), int(row["depth"]))


def open_variable_flow(path: str | os.PathLike[str]) -> VariableFlowReader:
    return VariableFlowReader(path)
