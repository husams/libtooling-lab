from __future__ import annotations

from typing import Any


def names(rows: Any) -> str:
    result = []
    for row in rows:
        result.append(
            str(row.get("name", ""))
            if isinstance(row, dict)
            else str(getattr(row, "qualified_name", ""))
        )
    return ",".join(result)


def forwarding(facade: Any, name: str, target: str) -> Any:
    if name in {"expression_occurrences", "expressions"}:
        return getattr(facade, name)(owner="app::run")
    if name in {"field_accesses", "field_writers", "field_writes"}:
        return getattr(facade, name)(target)
    return getattr(facade, name)("app::run", include_text=False)


def regions(cb: Any, lines: list[str]) -> None:
    refs = (("app::run", "function"), ("app::Box::flush", "method"))
    for ref, label in refs:
        result = cb.definition_regions(ref, include_text=True, max_bytes=32000)
        assert result.rows
        row = result.rows[0]
        if row.get("text"):
            lines.append(f"{label} region: {len(result.rows)} current bounded rows")
        else:
            assert row["freshness"] == "unavailable"
            lines.append(
                f"{label} region: typed unavailable ({row['unavailable_reason']})"
            )
    class_rows = [
        row
        for row in cb.source_regions(include_text=True, max_bytes=32000).rows
        if row["symbol"] == "app::Box" and row.get("text")
    ]
    assert class_rows
    lines.append(
        f"class region: exact app::Box ({len(class_rows)} current bounded rows)"
    )
