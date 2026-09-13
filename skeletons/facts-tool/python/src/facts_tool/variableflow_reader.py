import os
import sqlite3
from pathlib import Path

from .errors import fail
from .variableflow_decode import decode_run
from .variableflow_models import VariableFlowRun
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
        self._ensure_open()
        try:
            return tuple(
                decode_run(self._db, row)
                for row in self._db.execute(
                    "SELECT * FROM variable_flow_run ORDER BY run_id"
                )
            )
        except sqlite3.Error as error:
            fail("E_SOURCE", f"cannot read variable-flow database: {error}")

    def get(self, run_id: int) -> VariableFlowRun:
        self._ensure_open()
        if not isinstance(run_id, int) or isinstance(run_id, bool) or run_id < 1:
            fail("E_SOURCE", "run_id must be a positive integer")
        try:
            row = self._db.execute(
                "SELECT * FROM variable_flow_run WHERE run_id=?", (run_id,)
            ).fetchone()
            if row is None:
                fail("E_SOURCE", f"variable flow run {run_id} not found")
            return decode_run(self._db, row)
        except sqlite3.Error as error:
            fail("E_SOURCE", f"cannot read variable-flow database: {error}")

    def _ensure_open(self) -> None:
        if self._closed:
            fail("E_SOURCE", "variable-flow reader is closed")


def open_variable_flow(path: str | os.PathLike[str]) -> VariableFlowReader:
    return VariableFlowReader(path)
