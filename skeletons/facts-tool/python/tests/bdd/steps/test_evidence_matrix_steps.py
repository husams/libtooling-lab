import shutil
import sqlite3

from pytest_bdd import given, scenarios, then, when

scenarios("../features/evidence_matrix.feature")


@given("a schema13 matrix pair", target_fixture="matrix_cb")
def matrix_pair(schema13_pair):
    facts, project, source = schema13_pair
    facts_copy = source.parent.parent.parent / "matrix-facts.sqlite"
    project_copy = source.parent.parent.parent / "matrix-project.sqlite"
    shutil.copy2(facts, facts_copy)
    shutil.copy2(project, project_copy)
    with sqlite3.connect(facts_copy) as db:
        owner, target = (1 << 32) | 1, (1 << 32) | 5
        digest = db.execute(
            "SELECT source_sha256 FROM expression_occurrence WHERE occurrence_id=1"
        ).fetchone()[0]
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
                for index, access in enumerate(
                    ("read", "read_write", "escape", "none"), 3
                )
            ),
        )
        duplicate = list(
            db.execute("SELECT * FROM symbol WHERE id=?", (owner,)).fetchone()
        )
        duplicate[0], duplicate[6] = (1 << 32) | 100, "c:@F@run@duplicate#"
        db.execute(
            "INSERT INTO symbol VALUES(" + ",".join("?" for _ in duplicate) + ")",
            duplicate,
        )
    from facts_tool import open_codebase

    with open_codebase(facts_db=facts_copy, project_db=project_copy) as cb:
        yield cb


@when("I query every evidence access class")
def query_matrix(matrix_cb, world):
    accesses = ("read", "read_write", "escape", "none")
    world["matrix"] = {
        "expressions": matrix_cb.expressions(),
        "accesses": [
            matrix_cb.field_accesses("app::Box::value", access=value)
            for value in accesses
        ],
        "graph": matrix_cb.graph.expressions(),
        "ancestors": matrix_cb.graph.ancestors("app::Box"),
        "pages": [],
    }
    page = matrix_cb.expressions(limit=2)
    while True:
        world["matrix"]["pages"].extend(row["id"] for row in page)
        if not page.truncated:
            break
        page = matrix_cb.expressions(after_id=page.cursor, limit=2)


@then("every matrix facade exposes owners targets files and real ancestors")
def check_matrix(world, matrix_cb):
    matrix = world["matrix"]
    expressions = matrix["expressions"]
    assert {row["access"] for row in expressions} >= {
        "read",
        "write",
        "read_write",
        "escape",
        "none",
        "unknown",
    }
    assert {row["owner"] for row in expressions} == {"app::run"}
    assert {row["target"] for row in expressions} == {"app::Box::value"}
    assert all(row["file_id"] == 1 and row["file"] for row in expressions)
    assert matrix["pages"] == [row["id"] for row in expressions]
    assert matrix["graph"].rows and matrix["ancestors"][0].qualified_name == "app::Base"
    assert all(
        result.rows[0]["target"] == "app::Box::value" for result in matrix["accesses"]
    )
