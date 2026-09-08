import hashlib
import sqlite3
from pathlib import Path


def add_agent_source_regions(facts: Path, source: Path) -> None:
    digest = hashlib.sha256(source.read_bytes()).hexdigest()
    size = source.stat().st_size
    rows = (
        (
            2,
            "region-method",
            (1 << 32) | 6,
            1,
            1,
            0,
            0,
            size,
            digest,
            "method",
            "current",
            None,
        ),
        (
            3,
            "region-class",
            (1 << 32) | 4,
            1,
            1,
            0,
            0,
            size,
            digest,
            "class",
            "current",
            None,
        ),
    )
    with sqlite3.connect(facts) as database:
        database.executemany(
            "INSERT INTO source_region VALUES(?,?,?,?,?,?,?,?,?,?,?,?)", rows
        )
