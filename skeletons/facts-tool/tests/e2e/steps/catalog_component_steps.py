from __future__ import annotations

import json
import sqlite3
from pathlib import Path
from pytest_bdd import given, parsers, then
from support.catalog import Catalog
from support.database import require


@then("the core component and its directories and files are absent")
def component_removed(catalog: Catalog) -> None:
    require(catalog.rows("SELECT id FROM component WHERE id=?", (catalog.core_id,)) == [],
            "core component still exists")
    directory_ids = {row[0] for row in catalog.before["directory"] if row[1] == catalog.core_id}
    require(not any(row[0] in directory_ids for row in catalog.snapshot()["directory"]),
            "component directories were not deleted")
    require(not any(row[1] in directory_ids for row in catalog.snapshot()["file"]),
            "component files were not deleted")


@then("the neighbor component and its files are unchanged")
def neighbor_unchanged(catalog: Catalog) -> None:
    after = catalog.snapshot()
    neighbor = next(row for row in catalog.before["component"] if row[1] == "neighbor")
    require(neighbor in after["component"], "unrelated component changed")
    directories = [row for row in catalog.before["directory"] if row[1] == neighbor[0]]
    require(all(row in after["directory"] for row in directories), "neighbor directories changed")
    ids = {row[0] for row in directories}
    files = [row for row in catalog.before["file"] if row[1] in ids]
    require(all(row in after["file"] for row in files), "neighbor file identities changed")


@then("the external component is persisted at its requested root")
def external_component(catalog: Catalog) -> None:
    require(catalog.rows(
        "SELECT c.kind,CASE WHEN c.repository_id IS NOT NULL AND substr(c.path,1,1)!='/' "
        "THEN cl.path || '/' || c.path ELSE c.path END FROM component c "
        "LEFT JOIN repository r ON r.id=c.repository_id "
        "LEFT JOIN clone cl ON cl.id=r.active_clone_id WHERE c.name='vendor'")
        in ([("external", str(catalog.external))], [("external", str(catalog.external) + '/.')]),
        "component registration lost its kind or root")


@given(parsers.parse('the core component has version "{version}"'))
def versioned_component(catalog: Catalog, version: str) -> None:
    with sqlite3.connect(catalog.context.files_database_path) as connection:
        connection.execute("UPDATE component SET version=? WHERE id=?", (version, catalog.core_id))
    catalog.remember()


@then(parsers.re(r'the stored core version is "(?P<version>.*)"'))
def component_version(catalog: Catalog, version: str) -> None:
    require(catalog.rows("SELECT version FROM component WHERE id=?", (catalog.core_id,))
            == [(version or None,)], "component version was not updated or cleared")


@then("the exported commands match the stored core source commands")
def exported_commands(catalog: Catalog) -> None:
    commands = json.loads(catalog.stdout)
    require(isinstance(commands, list) and len(commands) == 3, "expected three core commands")
    expected = {str(path) for path in catalog.sources if path.is_relative_to(catalog.checkout / 'core')}
    require({entry['file'] for entry in commands} == expected, "export leaked or omitted sources")
    for entry in commands:
        require(Path(entry['directory']).is_dir(), "exported working directory is invalid")
        require(Path(entry['arguments'][0]) == catalog.context.compiler and
                '-std=c++23' in entry['arguments'] and entry['file'] in entry['arguments'],
                f"unusable exported command: {entry}")
