import sqlite3

from .callgraph_models import (
    CallGraphEdge,
    CallGraphRoot,
    CallGraphSite,
    CallGraphSymbol,
    CallGraphTarget,
)
from .errors import fail
from .ids import SymbolId
from .paths import FileResolver


def symbol(db: sqlite3.Connection, files: FileResolver, value: int) -> CallGraphSymbol:
    row = db.execute("SELECT * FROM symbol WHERE id=?", (value,)).fetchone()
    if row is None:
        fail("E_SCHEMA", f"call graph references missing symbol {value}")
    ident = SymbolId.unpack(int(row["id"]))
    return CallGraphSymbol(
        ident.packed,
        str(row["usr"]),
        str(row["qualified_name"]),
        ident.file_id,
        files.path(ident.file_id, required=False),
        row["line"],
        row["col"],
    )


def site(row: sqlite3.Row, files: FileResolver) -> CallGraphSite:
    receiver = row["receiver_type_id"]
    return CallGraphSite(
        int(row["file_id"]),
        files.path(int(row["file_id"]), required=False),
        row["line"],
        row["col"],
        int(row["offset"]),
        SymbolId.unpack(int(receiver)).packed if receiver is not None else None,
        row["certainty"],
        True,
    )


def root(row: sqlite3.Row, symbol_value: CallGraphSymbol) -> CallGraphRoot:
    return CallGraphRoot(symbol_value, str(row["usr"]))


def target(row: sqlite3.Row, symbol_value: CallGraphSymbol) -> CallGraphTarget:
    return CallGraphTarget(symbol_value, str(row["usr"]))


def edge(
    row: sqlite3.Row,
    source: CallGraphSymbol,
    target: CallGraphSymbol,
    evidence: list[sqlite3.Row],
    files: FileResolver,
) -> CallGraphEdge:
    kind_id = int(row["kind"])
    if kind_id not in (1, 18):
        fail("E_SCHEMA", f"unsupported persisted call graph relation kind {kind_id}")
    kind = "calls" if kind_id == 1 else "dispatch_calls"
    evidence_row = evidence[0] if evidence else None
    edge_site = (
        site(evidence_row, files)
        if evidence_row
        else CallGraphSite(
            int(row["file_id"]),
            files.path(int(row["file_id"]), required=False),
            None,
            None,
            int(row["offset"]),
            None,
            None,
        )
    )
    return CallGraphEdge(
        source,
        target,
        kind_id,
        kind,
        kind.title().replace("_", ""),
        int(row["position"]),
        int(row["file_id"]),
        files.path(int(row["file_id"]), required=False),
        edge_site.line if edge_site else None,
        edge_site.column if edge_site else None,
        int(row["offset"]),
        int(row["depth"]),
        bool(row["cycle"]),
        edge_site,
    )
