from __future__ import annotations

Table = list[list[str]]


def table_records(table: Table) -> list[dict[str, str]]:
    if len(table) < 2:
        raise ValueError("expected a table header and at least one data row")
    header = table[0]
    if not all(header) or len(set(header)) != len(header):
        raise ValueError(f"invalid table header: {header}")
    if any(len(row) != len(header) for row in table[1:]):
        raise ValueError(f"table row does not match header: {table}")
    return [dict(zip(header, row)) for row in table[1:]]
