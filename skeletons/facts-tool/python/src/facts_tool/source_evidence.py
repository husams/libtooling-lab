from __future__ import annotations

import hashlib
from pathlib import Path
from typing import TYPE_CHECKING

from .errors import fail
from .rows import Row
from .view_evidence import load_source_regions

if TYPE_CHECKING:
    from .executor import Executor


def prepare_source_regions(
    executor: Executor,
    symbol_id: int | None,
    include_text: bool,
    max_bytes: int,
    selected: list[Row] | None = None,
) -> tuple[list[Row], bool, bool]:
    rows = (
        load_source_regions(executor.loader.facts, executor.loader.files)
        if selected is None
        else selected
    )
    if symbol_id is not None:
        rows = [row for row in rows if int(row["symbol_id"]) == symbol_id]
    checked: list[Row] = []
    unknown = partial = False
    for source in rows:
        row = dict(source)
        _attach_text(row, max_bytes, include_text)
        if row["freshness"] == "unavailable":
            unknown = partial = True
        elif row["freshness"] == "stale":
            partial = True
        checked.append(row)
    return checked, unknown, partial


def _attach_text(row: Row, max_bytes: int, include_text: bool) -> None:
    if row["freshness"] != "current":
        return
    size, offset = int(row["size"]), int(row["offset"])
    if include_text and size > max_bytes:
        fail("E_LIMIT", "source region exceeds max_bytes")
    path_value = row.get("file")
    if not path_value:
        _unavailable(row, "source file path is unavailable")
        return
    path = Path(str(path_value))
    try:
        if _sha256(path) != str(row["source_sha256"]):
            _stale(row, "source fingerprint changed")
            return
        if offset < 0 or size < 0 or offset + size > path.stat().st_size:
            _stale(row, "source range exceeds source file")
            return
        if not include_text:
            return
        with path.open("rb") as source:
            source.seek(offset)
            data = source.read(size)
    except FileNotFoundError:
        _unavailable(row, "source file is missing")
        return
    except OSError as exc:
        _unavailable(row, f"source file is unreadable: {exc.strerror or exc}")
        return
    if len(data) != size:
        _stale(row, "source range exceeds source file")
        return
    try:
        row["text"] = data.decode("utf-8")
    except UnicodeDecodeError:
        _unavailable(row, "source region is not valid UTF-8")


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _unavailable(row: Row, reason: str) -> None:
    row["freshness"], row["unavailable_reason"] = "unavailable", reason


def _stale(row: Row, reason: str) -> None:
    row["freshness"], row["unavailable_reason"] = "stale", reason
