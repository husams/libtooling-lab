import sqlite3


def seed_evidence_rows(db: sqlite3.Connection, digest: str) -> int:
    owner, target = (1 << 32) | 1, (1 << 32) | 5
    db.executemany(
        "INSERT INTO expression_occurrence VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?)",
        (
            (
                index,
                f"occ-{index}",
                owner,
                target,
                1,
                1,
                1,
                0,
                1,
                digest,
                "MemberExpr",
                access,
                "current",
                None,
            )
            for index, access in enumerate(("read", "read_write", "escape", "none"), 3)
        ),
    )
    duplicate = list(db.execute("SELECT * FROM symbol WHERE id=?", (owner,)).fetchone())
    duplicate[0], duplicate[6] = (1 << 32) | 100, "c:@F@run@duplicate#"
    db.execute(
        "INSERT INTO symbol VALUES(" + ",".join("?" for _ in duplicate) + ")", duplicate
    )
    db.executemany(
        "INSERT INTO source_region VALUES(?,?,?,?,?,?,?,?,?,?,?,?)",
        (
            (
                index,
                f"region-{index}",
                owner,
                1,
                0,
                0,
                0,
                1,
                digest,
                kind,
                "unavailable",
                f"{kind} range unavailable",
            )
            for index, kind in enumerate(("declaration", "macro", "implicit"), 2)
        ),
    )
    db.execute(
        "INSERT INTO source_region VALUES(5,'region-big',?,?,?,?,?,?,?,?,?,?)",
        (owner, 1, 0, 0, 0, 10000, digest, "function", "current", None),
    )
    return owner
