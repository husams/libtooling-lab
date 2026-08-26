from __future__ import annotations

import shlex
import sqlite3
from pathlib import Path

from pytest_bdd import given, parsers, then, when
from support.database import require
from support.scenario import FactsToolContext


@given("a new catalog path and a Git checkout with a nested component")
def new_catalog(context: FactsToolContext) -> None:
    context.prepare()
    root = context.run_root_path / "new checkout"
    (root / "nested").mkdir(parents=True)
    result = context._run(["git", "init", "--quiet", str(root)])
    require(result.returncode == 0, context.last_output)


@when(parsers.re(r'I register the nested component as "(?P<kind>[^\"]+)" with "(?P<options>[^\"]*)"'))
def register_nested(context: FactsToolContext, kind: str, options: str) -> None:
    context._run([str(context.facts_tool), "component", "add", "--conf",
                  str(context.files_database_path), "--path",
                  str(context.run_root_path / "new checkout/nested"),
                  "--name", "sample", "--kind", kind, *shlex.split(options)])


@then(parsers.parse('registration creates a consistent catalog with root "{root}" and kind "{kind}"'))
def registered_root(context: FactsToolContext, root: str, kind: str) -> None:
    require(context.last_returncode == 0, context.last_output)
    with sqlite3.connect(context.files_database_path.as_uri() + "?mode=ro", uri=True) as db:
        rows = db.execute("SELECT c.path,c.kind,r.name,cl.path FROM component c "
                          "LEFT JOIN repository r ON r.id=c.repository_id "
                          "LEFT JOIN clone cl ON cl.id=r.active_clone_id WHERE c.name='sample'").fetchall()
        require(len(rows) == 1, f"component missing or duplicated: {rows}")
        path, actual_kind, repository, clone = rows[0]
        actual_root = Path(clone) / path if clone else Path(path)
        require(actual_root.resolve() == (context.run_root_path / "new checkout" / root).resolve(),
                f"wrong component root: {actual_root}")
        require(actual_kind == kind and repository == ("sample" if kind == "repo" else None),
                f"wrong kind or repository: {rows}")
        require(db.execute("SELECT count(*) FROM file").fetchone() == (0,),
                "registration unexpectedly imported source files")
        require(db.execute("PRAGMA foreign_key_check").fetchall() == [], "dangling foreign keys")
        require(db.execute("PRAGMA integrity_check").fetchall() == [("ok",)], "corrupt database")


@then("no source files or extracted facts have been created")
def no_extraction(context: FactsToolContext) -> None:
    require(not context.facts_database_path.exists(), "registration created a facts database")
    require(list((context.run_root_path / "new checkout/nested").iterdir()) == [],
            "registration created checkout files")


@when(parsers.parse('I query the missing catalog with "{command}"'))
def missing_catalog(context: FactsToolContext, command: str) -> None:
    context._run([str(context.facts_tool), *shlex.split(command),
                  "--conf", str(context.files_database_path)])


@then("the command fails without creating the missing configuration")
def missing_not_created(context: FactsToolContext) -> None:
    require(context.last_returncode == 1, context.last_output)
    require(not context.files_database_path.exists(), "query created a missing database")
