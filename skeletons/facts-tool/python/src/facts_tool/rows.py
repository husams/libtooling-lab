from typing import Any

Row = dict[str, Any]


def public_row(row: Row) -> Row:
    return {key: value for key, value in row.items() if not key.startswith("_")}


def row_key(row: Row) -> str:
    return str(row["_key"])


def short_name(qualified: str) -> str:
    leaf = qualified.rsplit("::", 1)[-1]
    return leaf.split("(", 1)[0]
