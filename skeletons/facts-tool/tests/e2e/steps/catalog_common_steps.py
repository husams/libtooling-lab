from __future__ import annotations

import json
from pathlib import Path
from pytest_bdd import given, parsers, then, when
from support.catalog import Catalog
from support.catalog_fixture import prepare_catalog
from support.scenario import FactsToolContext
from support.database import require


@when(parsers.parse('I run the catalog command "{command}"'))
def run_catalog_command(catalog: Catalog, command: str) -> None:
    catalog.run(command)


@then("the catalog command succeeds")
def catalog_succeeds(catalog: Catalog) -> None:
    require(catalog.context.last_returncode == 0, catalog.context.last_output)


@then(parsers.parse('the catalog output contains "{value}"'))
def output_contains(catalog: Catalog, value: str) -> None:
    require(value in catalog.stdout, catalog.context.last_output)


@then("the entire catalog is unchanged")
def catalog_unchanged(catalog: Catalog) -> None:
    require(catalog.snapshot() == catalog.before, "management changed catalog rows")


@then("the logical component directory and file rows are unchanged")
def logical_rows_unchanged(catalog: Catalog) -> None:
    after = catalog.snapshot()
    for table in ("component", "directory", "file"):
        require(after[table] == catalog.before[table], f"{table} identities changed")


@then("the original file rows are unchanged")
def files_unchanged(catalog: Catalog) -> None:
    require(catalog.snapshot()["file"] == catalog.before["file"], "file rows changed")


@then("the catalog database is consistent")
def catalog_consistent(catalog: Catalog) -> None:
    require(catalog.rows("PRAGMA integrity_check") == [("ok",)], "SQLite integrity failure")
    require(catalog.rows("PRAGMA foreign_key_check") == [], "dangling foreign keys")
    require(catalog.rows(
        "SELECT r.id FROM repository r LEFT JOIN clone c ON c.id=r.active_clone_id "
        "WHERE r.active_clone_id IS NOT NULL AND (c.id IS NULL OR c.repository_id!=r.id)"
    ) == [], "active clone belongs to a missing or different repository")


@then("the catalog contains the imported source commands and directories")
def imported_rows(catalog: Catalog) -> None:
    rows = catalog.rows("SELECT name,driver,compile_options FROM file ORDER BY name")
    require([row[0] for row in rows] == ["four.cpp", "one.cpp", "three.cpp", "two.cpp"],
            f"unexpected source rows: {rows}")
    require(all(Path(driver) == catalog.context.compiler and
                "-std=c++23" in json.loads(options) for _, driver, options in rows),
            "import lost compiler drivers or compile flags")
    require({row[2] for row in catalog.before["directory"]} ==
            {"src", "src/deep", "src-neighbor", "other-only"}, "wrong directory identities")


@given("an imported catalog with two repositories and independent components", target_fixture="catalog")
def imported_catalog(context: FactsToolContext) -> Catalog:
    return prepare_catalog(context)
