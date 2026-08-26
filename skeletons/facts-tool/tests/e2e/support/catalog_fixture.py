from __future__ import annotations

import json
import shutil
import sqlite3
from support.catalog import Catalog
from support.database import file_snapshot, require
from support.scenario import FactsToolContext


def prepare_catalog(context: FactsToolContext) -> Catalog:
    context.prepare()
    checkout = context.run_root_path / "checkout with spaces"
    catalog = Catalog(context, checkout, context.run_root_path / "second checkout",
                      context.run_root_path / "external headers")
    catalog.external.mkdir()
    paths = ("core/src/one.cpp", "core/src/deep/two.cpp",
             "core/src-neighbor/three.cpp", "neighbor/other-only/four.cpp")
    for number, relative in enumerate(paths):
        source = checkout / relative
        source.parent.mkdir(parents=True, exist_ok=True)
        content = f"int catalog_value_{number}() {{ return {number}; }}\n".encode()
        source.write_bytes(content)
        catalog.sources[source] = content
    commands = [
        {"directory": str(source.parent), "file": str(source),
         "arguments": [str(context.compiler), "-std=c++23", "-c", str(source)]}
        for source in catalog.sources
    ]
    compilation_database = context.run_root_path / "compile_commands.json"
    compilation_database.write_text(json.dumps(commands), encoding="utf-8")
    result = context._run([
        str(context.facts_tool), "import", "--conf", str(context.files_database_path),
        "--compilation-database", str(context.run_root_path),
        "--component", f"core={checkout / 'core'}",
        "--component", f"neighbor={checkout / 'neighbor'}",
    ])
    require(result.returncode == 0, context.last_output)
    require(len(catalog.rows("SELECT id FROM file WHERE driver IS NOT NULL")) == 4,
            "the real import did not persist all four source commands")
    imported_paths = file_snapshot(context.files_database_path)

    # The current import CLI has no --repo option. Seed only repository/clone
    # ownership as Given state; never seed the outcome of a management command.
    # Components, directories, files and commands are all created by real import.
    with sqlite3.connect(context.files_database_path) as connection:
        connection.execute("PRAGMA foreign_keys=ON")
        # Import can auto-discover repositories differently inside a git checkout
        # and in an isolated source snapshot. Replace only ownership Given state.
        connection.execute("UPDATE component SET repository_id=NULL")
        connection.execute("DELETE FROM clone")
        connection.execute("DELETE FROM repository")
        for name, component in (("demo", "core"), ("independent", "neighbor")):
            repo_id = connection.execute(
                "INSERT INTO repository(name) VALUES(?)", (name,)
            ).lastrowid
            clone_path = checkout if component == "core" else checkout / "neighbor"
            clone_id = connection.execute(
                "INSERT INTO clone(repository_id,path,label) VALUES(?,?,?)",
                (repo_id, str(clone_path), "original"),
            ).lastrowid
            connection.execute("UPDATE repository SET active_clone_id=? WHERE id=?",
                               (clone_id, repo_id))
            connection.execute("UPDATE component SET path=?,repository_id=? WHERE name=?",
                               ("core" if component == "core" else ".", repo_id, component))
    require(file_snapshot(context.files_database_path) == imported_paths,
            "repository fixture setup changed imported file identities or paths")
    shutil.copytree(checkout, catalog.second)
    catalog.sources.update({catalog.second / path.relative_to(checkout): content
                            for path, content in list(catalog.sources.items())})
    result = context._run([
        str(context.facts_tool), "extract", "--conf", str(context.files_database_path),
        "--output", str(context.facts_database_path),
        str(checkout / "core/src/one.cpp"),
    ])
    require(result.returncode == 0, context.last_output)
    catalog.facts_before = context.facts_database_path.read_bytes()
    compilation_database.unlink()
    catalog.remember()
    return catalog
