import sqlite3

from .catalog_relations import relation_id, relation_name
from .ids import SymbolId
from .rows import Row, row_key
from .view_loader import ViewLoader


class Neighbors:
    def __init__(self, facts: sqlite3.Connection, loader: ViewLoader):
        self.facts, self.loader = facts, loader

    def __call__(self, rows: list[Row], relation: str, inbound: bool) -> list[Row]:
        name = relation_name(relation)
        stored_id = relation_id(name)
        if stored_id is not None:
            return self._stored(rows, stored_id, inbound)
        return self._pseudo(rows, name, inbound)

    def _stored(self, rows: list[Row], kind: int, inbound: bool) -> list[Row]:
        symbols = {int(row["id"]): row for row in self.loader.load("symbol")}
        result: list[Row] = []
        column, target = (
            ("destination_id", "source_id")
            if inbound
            else ("source_id", "destination_id")
        )
        sql = (
            f"SELECT {target} FROM relation WHERE {column}=? AND kind=? "
            "ORDER BY position," + target
        )
        for row in rows:
            if row.get("_view") != "symbol":
                continue
            for edge in self.facts.execute(sql, (row["_db_id"], kind)):
                key = SymbolId.unpack(int(edge[0])).packed
                if key in symbols:
                    result.append(symbols[key])
        return _unique(result)

    def _pseudo(self, rows: list[Row], name: str, inbound: bool) -> list[Row]:
        mapping = {
            "has_parameter": "parameter",
            "has_template_parameter": "template_parameter",
            "has_template_argument": "template_argument",
            "definition": "definition",
        }
        if name == "includes":
            return self._includes(rows, inbound)
        if name == "declaration":
            return self._owned(rows, "definition", not inbound)
        return self._owned(rows, mapping[name], inbound)

    def _owned(self, rows: list[Row], view: str, inbound: bool) -> list[Row]:
        details = self.loader.load(view)
        symbols = {int(row["id"]): row for row in self.loader.load("symbol")}
        result: list[Row] = []
        if inbound:
            for row in rows:
                owner = row.get("owner_id", row.get("symbol_id"))
                if owner in symbols:
                    result.append(symbols[int(owner)])
        else:
            owners = {int(row["id"]) for row in rows if row.get("_view") == "symbol"}
            result.extend(
                row
                for row in details
                if row.get("owner_id", row.get("symbol_id")) in owners
            )
        return _unique(result)

    def _includes(self, rows: list[Row], inbound: bool) -> list[Row]:
        files = {int(row["id"]): row for row in self.loader.load("file")}
        source, target = (
            ("dst_file_id", "src_file_id")
            if inbound
            else ("src_file_id", "dst_file_id")
        )
        sql = (
            f"SELECT {target} FROM include_dependency "
            f"WHERE {source}=? ORDER BY {target}"
        )
        result = []
        for row in rows:
            for edge in self.facts.execute(sql, (row["id"],)):
                if int(edge[0]) in files:
                    result.append(files[int(edge[0])])
        return _unique(result)


def _unique(rows: list[Row]) -> list[Row]:
    return list({row_key(row): row for row in rows}.values())
