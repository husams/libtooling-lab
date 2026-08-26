from __future__ import annotations

import sqlite3
from pytest_bdd import given, parsers, then
from support.catalog import Catalog
from support.database import require


@then("the catalog command rejects the unknown object")
def unknown_object(catalog: Catalog) -> None:
    require(catalog.context.last_returncode == 1, catalog.context.last_output)
    diagnostic = catalog.context.last_output.lower()
    require(any(word in diagnostic for word in ("not found", "unknown repository", "unknown clone",
                                                "unknown component", "unknown directory")),
            f"missing object-specific error (parser failures do not count): {diagnostic}")


@given("SQLite rejects deletion of the core source files")
def reject_deletion(catalog: Catalog) -> None:
    with sqlite3.connect(catalog.context.files_database_path) as connection:
        connection.execute(
            "CREATE TRIGGER reject_catalog_delete BEFORE DELETE ON file "
            "WHEN OLD.name='one.cpp' BEGIN "
            "SELECT RAISE(ABORT, 'forced catalog deletion failure'); END")


@then("the catalog command reports the SQLite deletion failure")
def deletion_failed(catalog: Catalog) -> None:
    require(catalog.context.last_returncode == 1 and
            'forced catalog deletion failure' in catalog.context.last_output,
            f"missing SQLite failure: {catalog.context.last_output}")


@then(parsers.parse('the catalog command fails with "{diagnostic}"'))
def command_failed(catalog: Catalog, diagnostic: str) -> None:
    require(catalog.context.last_returncode == 1, catalog.context.last_output)
    require(diagnostic in catalog.context.last_output, catalog.context.last_output)


@then("the catalog parser rejects the invalid arguments")
def invalid_arguments(catalog: Catalog) -> None:
    require(catalog.context.last_returncode not in (0, 1), catalog.context.last_output)
    require("usage" in catalog.context.last_output.lower() or
            "help" in catalog.context.last_output.lower(), catalog.context.last_output)


@given("SQLite rejects the final repository deletion")
def reject_repository_deletion(catalog: Catalog) -> None:
    with sqlite3.connect(catalog.context.files_database_path) as connection:
        connection.execute(
            "CREATE TRIGGER reject_repository_delete BEFORE DELETE ON repository "
            "WHEN OLD.name='demo' BEGIN "
            "SELECT RAISE(ABORT, 'forced repository deletion failure'); END")
    catalog.remember()
