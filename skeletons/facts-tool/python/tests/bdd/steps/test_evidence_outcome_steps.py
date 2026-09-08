import hashlib
import shutil
import sqlite3
from pathlib import Path

from pytest_bdd import given, scenarios, then, when

scenarios("../features/evidence_outcomes.feature")


@given("a schema13 outcome pair", target_fixture="outcome_cb")
def outcome_pair(schema13_pair):
    facts, project, source = schema13_pair
    facts_copy = source.parent.parent.parent / "outcome-facts.sqlite"
    project_copy = source.parent.parent.parent / "outcome-project.sqlite"
    shutil.copy2(facts, facts_copy)
    shutil.copy2(project, project_copy)
    invalid = source.parent / "invalid.bin"
    invalid.write_bytes(b"\xff")
    with sqlite3.connect(project_copy) as db:
        db.execute("UPDATE file SET name='invalid.bin' WHERE id=2")
    with sqlite3.connect(facts_copy) as db:
        owner = (1 << 32) | 1
        digest = hashlib.sha256(source.read_bytes()).hexdigest()
        invalid_digest = hashlib.sha256(b"\xff").hexdigest()
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
            "INSERT INTO source_region VALUES(5,'region-invalid',?,?,?,?,?,?,?,?,?,?)",
            (owner, 2, 0, 0, 0, 1, invalid_digest, "function", "current", None),
        )
        db.execute(
            "INSERT INTO source_region VALUES(6,'region-big',?,?,?,?,?,?,?,?,?,?)",
            (owner, 1, 0, 0, 0, 10000, digest, "function", "current", None),
        )
        db.execute(
            "INSERT INTO facts_project_provenance VALUES(2,?,'demo')", (str(invalid),)
        )
    from facts_tool import open_codebase

    with open_codebase(facts_db=facts_copy, project_db=project_copy) as cb:
        yield cb


@when("I query bounded and unavailable source outcomes")
def query_outcomes(outcome_cb, world):
    project = outcome_cb.executor.loader.project
    source = (
        Path(project.execute("SELECT path FROM clone WHERE id=1").fetchone()[0])
        / "src"
        / "main.cpp"
    )
    world["outcome"] = (outcome_cb, source)


@then("source paging and availability outcomes are explicit")
def check_outcomes(world):
    cb, source = world["outcome"]
    page = cb.source_regions("app::run", limit=1, include_text=True, max_bytes=30)
    assert page.truncated and page.cursor == "1" and page.rows[0]["text"]
    unavailable = cb.source_regions("app::run", after_id=1)
    assert {row["symbol_kind"] for row in unavailable} >= {
        "declaration",
        "macro",
        "implicit",
    }
    invalid = cb.source_regions("app::run", include_text=True, after_id=4)
    assert invalid.unknown and invalid.rows[0]["freshness"] == "unavailable"
    source.write_text("changed\n", encoding="utf-8")
    assert cb.expression_occurrences("app::run").partial
