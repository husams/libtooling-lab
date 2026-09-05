from pytest_bdd import given, parsers, then, when

from support.batch import BatchProject


@given(
    "an imported batch project with shared headers and spaced filenames",
    target_fixture="batch_project",
)
def project(context, tmp_path):
    return BatchProject(context, tmp_path)


@when(parsers.parse('I batch "{mode}" using {jobs:d} jobs and "{inputs}" inputs'))
def run_batch(batch_project, mode, jobs, inputs):
    batch_project.batch(mode, jobs, inputs)


@then(parsers.parse('every source has valid "{mode}" results in its own database'))
def verify_batch(batch_project, mode):
    batch_project.verify(mode)


@when("I repeat extraction and dependency batching in the same output directory")
def repeat(batch_project):
    for mode in ("extract", "dependency", "extract", "dependency"):
        batch_project.batch(mode, 3, "list")
        batch_project.verify(mode)


@then("both symbols and dependencies remain readable after repeated batching")
def verify_both(batch_project):
    batch_project.verify("extract")
    batch_project.verify("dependency")


@when("one batch source becomes invalid before extraction")
def fail_source(batch_project):
    batch_project.sources[0].write_text("#error intentional batch failure\n")
    batch_project.batch("extract", 3, "compdb")


@then("batching reports failure and retains successful source results and diagnostics")
def verify_failure(batch_project):
    assert batch_project.result.returncode != 0
    logs = list(batch_project.output.glob("*.log"))
    assert any("intentional batch failure" in path.read_text() for path in logs)
    import sqlite3

    populated = 0
    for path in batch_project.output.glob("*.db"):
        with sqlite3.connect(path) as db:
            populated += bool(db.execute("SELECT 1 FROM symbol LIMIT 1").fetchone())
    assert populated == len(batch_project.sources) - 1
