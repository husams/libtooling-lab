import json

from .errors import fail
from .fields import FIELDS
from .rows import Row, public_row, row_key


def select_rows(rows: list[Row], view: str, fields: tuple[str, ...]) -> list[Row]:
    unknown = [field for field in fields if field not in FIELDS.get(view, set())]
    if unknown:
        fail("E_FIELD", f"unknown {view} field(s): {', '.join(unknown)}")
    return [
        {field: row.get(field) for field in fields} | {"_key": row_key(row)}
        for row in rows
    ]


def distinct_rows(rows: list[Row]) -> list[Row]:
    result: dict[str, Row] = {}
    for row in rows:
        key = json.dumps(public_row(row), sort_keys=True, default=str)
        result.setdefault(key, row)
    return list(result.values())


def order_rows(rows: list[Row], fields: tuple[str, ...]) -> list[Row]:
    available = set().union(*(row.keys() for row in rows)) if rows else set(fields)
    unknown = [field for field in fields if field not in available]
    if unknown:
        fail("E_FIELD", f"cannot order by: {', '.join(unknown)}")

    def key(row: Row) -> tuple[object, ...]:
        values = tuple(
            (row.get(field) is None, str(row.get(field))) for field in fields
        )
        return (*values, str(row.get("_key", "")))

    return sorted(rows, key=key)
