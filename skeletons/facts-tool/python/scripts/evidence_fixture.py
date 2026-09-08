import hashlib
import sqlite3
from pathlib import Path

from evidence_seed import seed_evidence_rows


def prepare_evidence(facts: Path, project: Path) -> None:
    with sqlite3.connect(facts) as db, sqlite3.connect(project) as project_db:
        source = Path(
            db.execute(
                "SELECT path FROM facts_project_provenance WHERE file_id=1"
            ).fetchone()[0]
        )  # noqa: E501
        project_db.execute(
            "UPDATE clone SET path=? WHERE id=1", (str(source.parent.parent),)
        )
        project_db.execute("UPDATE file SET name='save.cpp' WHERE id=2")
        db.execute("DELETE FROM expression_occurrence WHERE occurrence_id>=3")
        db.execute("DELETE FROM source_region WHERE region_id>=2")
        db.execute("DELETE FROM symbol WHERE id=?", ((1 << 32) | 100,))
        db.execute("DELETE FROM facts_project_provenance WHERE file_id=2")
        original = "int run() { return π; }\n".encode()
        source.write_bytes(original)
        digest = hashlib.sha256(original).hexdigest()
        db.execute(
            "UPDATE source_region SET offset=0,size=?,source_sha256=?,"
            "freshness='current',unavailable_reason=NULL WHERE region_id=1",
            (len(original), digest),
        )
        owner = seed_evidence_rows(db, digest)
        invalid = source.parent / "invalid.bin"
        invalid.write_bytes(b"\xff")
        project_db.execute("UPDATE file SET name='invalid.bin' WHERE id=2")
        db.execute(
            "INSERT INTO source_region VALUES(6,'region-invalid',?,?,?,?,?,?,?,?,?,?)",
            (
                owner,
                2,
                0,
                0,
                0,
                1,
                hashlib.sha256(b"\xff").hexdigest(),
                "function",
                "current",
                None,
            ),
        )
        db.execute(
            "INSERT INTO facts_project_provenance VALUES(2,?,'demo')", (str(invalid),)
        )
