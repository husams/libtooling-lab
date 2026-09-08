from __future__ import annotations

from typing import TYPE_CHECKING

from .rows import Row

if TYPE_CHECKING:
    from .executor import Executor


def prepare_expression_occurrences(
    executor: Executor, rows: list[Row]
) -> tuple[list[Row], bool, bool]:
    from .source_evidence import _attach_text

    checked: list[Row] = []
    unknown = partial = False
    for source in rows:
        row = dict(source)
        _attach_text(row, 0, False)
        if row["freshness"] == "unavailable":
            unknown = partial = True
        elif row["freshness"] == "stale":
            partial = True
        checked.append(row)
    return checked, unknown, partial
