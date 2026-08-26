from __future__ import annotations

import sqlite3
from pytest_bdd import given, then, when
from support.catalog import Catalog
from support.database import file_snapshot, require


@given("a registered second clone of the demo repository")
def second_clone(catalog: Catalog) -> None:
    # Given state avoids depending on the separate add-clone scenario.
    with sqlite3.connect(catalog.context.files_database_path) as connection:
        connection.execute(
            "INSERT INTO clone(repository_id,path,label) "
            "SELECT id,?,'second' FROM repository WHERE name='demo'", (str(catalog.second),))
    catalog.remember()


@then("the second clone is registered but the original clone is active")
def registered_clone(catalog: Catalog) -> None:
    require(catalog.rows(
        "SELECT c.path,c.label FROM clone c JOIN repository r ON r.id=c.repository_id "
        "WHERE r.name='demo' ORDER BY c.id") ==
        [(str(catalog.checkout), "original"), (str(catalog.second), "second")],
        "clone registration did not persist exactly one new clone")
    require(catalog.rows(
        "SELECT c.path FROM repository r JOIN clone c ON c.id=r.active_clone_id "
        "WHERE r.name='demo'") == [(str(catalog.checkout),)], "registration switched clones")


@then("the active clone is the second checkout")
def switched_clone(catalog: Catalog) -> None:
    require(catalog.rows(
        "SELECT c.path FROM repository r JOIN clone c ON c.id=r.active_clone_id "
        "WHERE r.name='demo'") == [(str(catalog.second),)], "clone switch was not persisted")


@when("I extract a source from the second checkout using only the stored database")
def extract_second_clone(catalog: Catalog) -> None:
    context = catalog.context
    result = context._run([
        str(context.facts_tool), "extract", "--conf", str(context.files_database_path),
        "--output", str(context.run_root_path / "switched-facts.sqlite"),
        str(catalog.second / "core/src/one.cpp"),
    ])
    require(result.returncode == 0, context.last_output)


@then("extraction from the second checkout persists the expected symbol")
def switched_facts(catalog: Catalog) -> None:
    path = catalog.context.run_root_path / "switched-facts.sqlite"
    with sqlite3.connect(path.as_uri() + "?mode=ro", uri=True) as connection:
        rows = connection.execute(
            "SELECT ((id >> 32) & 4294967295) FROM symbol "
            "WHERE qualified_name LIKE 'catalog_value_0%'").fetchall()
    file_id = catalog.rows("SELECT id FROM file WHERE name='one.cpp'")[0][0]
    require(rows == [(file_id,)], f"new clone did not preserve the source identity: {rows}")


@then("the demo repository and its clones are absent")
def repository_removed(catalog: Catalog) -> None:
    repo_id = next(row[0] for row in catalog.before["repository"] if row[1] == "demo")
    require(catalog.rows("SELECT id FROM repository WHERE id=?", (repo_id,)) == [],
            "repository still exists")
    require(catalog.rows("SELECT id FROM clone WHERE repository_id=?", (repo_id,)) == [],
            "repository clones still exist")


@then("the core component is detached without changing resolved file paths")
def detached_component(catalog: Catalog) -> None:
    require(catalog.rows("SELECT repository_id FROM component WHERE id=?", (catalog.core_id,))
            == [(None,)], "component is not detached")
    require(file_snapshot(catalog.context.files_database_path) == catalog.resolved_before,
            "detaching the repository broke component/file path resolution")
