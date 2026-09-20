from typing import Any

Row = dict[str, Any]


class ProjectedRow(dict[str, Any]):
    def __init__(self, values: Row, cursor: str | None):
        super().__init__(values)
        self.cursor = cursor


def row_cursor(row: Row) -> str | None:
    value = (
        row.cursor if isinstance(row, ProjectedRow) else row.get("id", row.get("_key"))
    )
    return None if value is None else str(value)


def public_row(row: Row) -> Row:
    return {key: value for key, value in row.items() if not key.startswith("_")}


def row_key(row: Row) -> str:
    return str(row["_key"])


def short_name(qualified: str) -> str:
    leaf = qualified.rsplit("::", 1)[-1]
    return leaf.split("(", 1)[0]
