from __future__ import annotations

import json
import sqlite3

from pytest_bdd import given, then
from support.catalog import Catalog
from support.database import require


@given("the catalog is read-only")
def read_only_catalog(catalog: Catalog):
    path = catalog.context.files_database_path
    path.chmod(0o444)
    try:
        yield
    finally:
        path.chmod(0o644)


@then("the catalog database bytes are unchanged")
def unchanged_bytes(catalog: Catalog) -> None:
    require(catalog.context.files_database_path.read_bytes() == catalog.configuration_before,
            "read-only operation modified the configuration file")


@given("the second clone is missing an imported source")
def incomplete_clone(catalog: Catalog) -> None:
    source = catalog.second / "core/src/deep/two.cpp"
    source.unlink()
    del catalog.sources[source]


@given("two components contain the same indexed directory path")
def ambiguous_directory(catalog: Catalog) -> None:
    with sqlite3.connect(catalog.context.files_database_path) as connection:
        connection.execute("INSERT INTO directory(component_id,path) "
                           "SELECT id,'src/deep' FROM component WHERE name='neighbor'")
    catalog.remember()


@given("two components have the same name")
def ambiguous_component(catalog: Catalog) -> None:
    with sqlite3.connect(catalog.context.files_database_path) as connection:
        connection.execute("UPDATE component SET name='core' WHERE name='neighbor'")
    catalog.remember()


@given("an unregistered directory inside the active checkout")
def unregistered_directory(catalog: Catalog) -> None:
    (catalog.checkout / "extension").mkdir()


@then("the new component is relative to the demo checkout")
def relative_component(catalog: Catalog) -> None:
    require(catalog.rows("SELECT c.path,r.name FROM component c "
                        "JOIN repository r ON r.id=c.repository_id WHERE c.name='extension'")
            == [("extension", "demo")], "component was not attached using a relative path")


@then("the exported command list is empty")
def empty_export(catalog: Catalog) -> None:
    require(json.loads(catalog.stdout) == [], "empty component exported unrelated commands")


@then("only one second clone is registered")
def unique_clone(catalog: Catalog) -> None:
    require(catalog.rows("SELECT count(*) FROM clone WHERE path=?", (str(catalog.second),))
            == [(1,)], "re-registering a clone created duplicate rows")
