from __future__ import annotations

import json
import sqlite3
from pathlib import Path

from pytest_bdd import given, parsers, then, when

from support.catalog import Catalog
from support.database import require


def manual_path(catalog: Catalog) -> Path:
    return catalog.checkout / "core/src/deep/manual.cpp"


def nested_path(catalog: Catalog) -> Path:
    return catalog.checkout / "core/src/newdir/one.cpp"


@given("a manual source inside the deepest indexed directory")
def manual_source(catalog: Catalog) -> None:
    path = manual_path(catalog)
    content = b"int manual_value() { return 14; }\n"
    path.write_bytes(content)
    catalog.sources[path] = content


@given("an existing source outside every indexed directory")
def outside_source(catalog: Catalog) -> None:
    path = catalog.external / "outside.cpp"
    content = b"int outside_value() { return 7; }\n"
    path.write_bytes(content)
    catalog.sources[path] = content


@given("a source beneath an unregistered child directory with an existing basename")
def nested_source(catalog: Catalog) -> None:
    path = nested_path(catalog)
    path.parent.mkdir()
    content = b"int nested_value() { return 21; }\n"
    path.write_bytes(content)
    catalog.sources[path] = content


@then("the nested file has its own registered directory and round-trips exactly")
def nested_file_round_trips(catalog: Catalog) -> None:
    rows = catalog.rows(
        "SELECT d.path,f.name,f.working_directory FROM file f "
        "JOIN directory d ON d.id=f.directory_id WHERE f.name='one.cpp' "
        "ORDER BY d.path")
    require(("src/newdir", "one.cpp", str(nested_path(catalog).parent)) in rows,
            f"nested file path was collapsed: {rows}")
    require(any(path == "src" and name == "one.cpp"
                for path, name, _ in rows),
            f"existing basename was displaced: {rows}")


@then("the nested file appears at its exact path")
def nested_file_output(catalog: Catalog) -> None:
    require(str(nested_path(catalog)) in catalog.stdout,
            f"exact nested path is absent: {catalog.stdout}")


@given("a registered manual source")
def registered_manual(catalog: Catalog) -> None:
    manual_source(catalog)
    catalog.run("file add {manual-file} --driver {compiler}")
    require(catalog.context.last_returncode == 0, catalog.context.last_output)
    catalog.remember()


@given("a registered manual source with overridden compilation options")
def overridden_manual(catalog: Catalog) -> None:
    manual_source(catalog)
    # Use import's native ownership model for the import-refresh scenarios;
    # the shared catalog fixture deliberately rewrites ownership to exercise
    # multi-repository management commands.
    catalog.context.files_database_path.unlink()
    manual_path(catalog).unlink()
    write_import(catalog, False)
    manual_source(catalog)
    catalog.run("file add {manual-file} --driver {compiler} "
                "--arg=-DMANUAL --arg=-I --arg=manual/include")
    require(catalog.context.last_returncode == 0, catalog.context.last_output)
    catalog.remember()


@then("the manual file is stored under the deepest directory with exact compilation details")
def manual_details(catalog: Catalog) -> None:
    row = catalog.rows(
        "SELECT d.path,f.driver,f.working_directory,f.compile_options,"
        "f.args_overridden FROM file f JOIN directory d ON d.id=f.directory_id "
        "WHERE f.name='manual.cpp'")[0]
    require(row[0] == "src/deep", f"wrong owning directory: {row}")
    require(Path(row[1]) == catalog.context.compiler, f"wrong driver: {row}")
    require(Path(row[2]) == manual_path(catalog).parent, f"wrong default cwd: {row}")
    require(json.loads(row[3]) == ["-DVALUE=1", "-I", "include"],
            f"arguments were not exact: {row}")
    require(row[4] == 1, f"manual command not marked overridden: {row}")


@then("the manual file uses the explicit working directory")
def explicit_working_directory(catalog: Catalog) -> None:
    stored = catalog.rows("SELECT working_directory FROM file WHERE name='manual.cpp'")
    require(stored == [(str((catalog.checkout / "core/src").resolve()),)],
            f"working directory was not canonicalized: {stored}")


@then("the manual file row is absent but its source remains")
def manual_removed(catalog: Catalog) -> None:
    require(catalog.rows("SELECT id FROM file WHERE name='manual.cpp'") == [],
            "manual file row remains")
    require(manual_path(catalog).is_file(), "source was removed from checkout")


@given("the catalog contains no file rows")
def empty_catalog_files(catalog: Catalog) -> None:
    with sqlite3.connect(catalog.context.files_database_path) as connection:
        connection.execute("DELETE FROM file")
    catalog.remember()


@given("repeated compile options on matching and nonmatching files")
def repeated_options(catalog: Catalog) -> None:
    options = json.dumps(["-I", "include", "-DKEEP", "-I", "include"])
    with sqlite3.connect(catalog.context.files_database_path) as connection:
        connection.execute("UPDATE file SET compile_options=?,args_overridden=0", (options,))
    catalog.remember()


def stored_options(catalog: Catalog) -> dict[str, tuple[list[str], int]]:
    return {name: (json.loads(options), overridden) for name, options, overridden in
            catalog.rows("SELECT d.path||'/'||f.name,f.compile_options,"
                         "f.args_overridden FROM file f JOIN directory d "
                         "ON d.id=f.directory_id ORDER BY f.id")}


@then("matching files contain the exact option sequence once and nonmatching files are unchanged")
def set_sequence(catalog: Catalog) -> None:
    rows = stored_options(catalog)
    for path, (options, overridden) in rows.items():
        count = sum(options[index:index + 2] == ["-I", "include"]
                    for index in range(len(options) - 1))
        if path.startswith("src/"):
            require(count == 1 and options[-2:] == ["-I", "include"] and overridden == 1,
                    f"matching options incorrect for {path}: {options}")
        else:
            require(options == ["-I", "include", "-DKEEP", "-I", "include"] and
                    overridden == 0, f"nonmatch changed for {path}: {options}")


@then("matching files contain no exact option sequence and nonmatching files are unchanged")
def clear_sequence(catalog: Catalog) -> None:
    rows = stored_options(catalog)
    for path, (options, overridden) in rows.items():
        if path.startswith("src/"):
            require(options == ["-DKEEP"] and overridden == 1,
                    f"matching sequence remains for {path}: {options}")
        else:
            require(options == ["-I", "include", "-DKEEP", "-I", "include"] and
                    overridden == 0, f"nonmatch changed for {path}: {options}")


def write_import(catalog: Catalog, include_manual: bool) -> None:
    sources = [path for path in catalog.sources
               if path.is_relative_to(catalog.checkout) and
               path.name != "manual.cpp"]
    if include_manual:
        sources.append(manual_path(catalog))
    commands = [{"directory": str(source.parent), "file": str(source),
                 "arguments": [str(catalog.context.compiler), "-std=c++23",
                               "-DREIMPORTED", "-c", str(source)]}
                for source in sources]
    (catalog.context.run_root_path / "compile_commands.json").write_text(
        json.dumps(commands), encoding="utf-8")
    result = catalog.context._run([
        str(catalog.context.facts_tool), "import", "--conf",
        str(catalog.context.files_database_path), "--compilation-database",
        str(catalog.context.run_root_path), "--component",
        f"core={catalog.checkout / 'core'}", "--component",
        f"neighbor={catalog.checkout / 'neighbor'}"])
    require(result.returncode == 0, catalog.context.last_output)


@when("I reimport commands that omit the manual source")
def reimport_omitted(catalog: Catalog) -> None:
    write_import(catalog, False)


@when("I reimport commands that include the manual source")
def reimport_included(catalog: Catalog) -> None:
    write_import(catalog, True)


@then("the manual file and every compilation field are preserved")
def omitted_preserved(catalog: Catalog) -> None:
    before = next(row for row in catalog.before["file"] if row[2] == "manual.cpp")
    after = catalog.rows("SELECT * FROM file WHERE name='manual.cpp'")
    require(after == [before], f"omitted manual row changed: {before} -> {after}")


@then("the manual file command is replaced and its override is cleared")
def included_replaced(catalog: Catalog) -> None:
    rows = catalog.rows("SELECT driver,working_directory,compile_options,"
                        "args_overridden FROM file WHERE name='manual.cpp'")
    require(len(rows) == 1, f"manual identity was not retained: {rows}")
    driver, working, options, overridden = rows[0]
    require(Path(driver) == catalog.context.compiler and
            working == "<core>/src/deep" and
            "-DREIMPORTED" in json.loads(options) and overridden == 0,
            f"included manual command was not replaced: {rows}")


@then("the second clone registration is absent while both checkouts remain")
def clone_registration_removed(catalog: Catalog) -> None:
    require(catalog.rows("SELECT id FROM clone WHERE label='second'") == [],
            "non-active clone remains registered")
    require(catalog.checkout.is_dir() and catalog.second.is_dir(),
            "clone removal deleted a checkout")
    active = catalog.rows("SELECT c.label FROM repository r JOIN clone c "
                          "ON c.id=r.active_clone_id WHERE r.name='demo'")
    require(active == [("original",)], f"active clone changed: {active}")


@when(parsers.parse('I run the symbol command "{command}"'))
def run_symbol(catalog: Catalog, command: str) -> None:
    catalog.run_symbol(command)


@when("I show the extracted catalog function")
def show_known_symbol(catalog: Catalog) -> None:
    with sqlite3.connect(catalog.context.facts_database_path) as connection:
        name = connection.execute("SELECT qualified_name FROM symbol WHERE "
                                  "qualified_name LIKE 'catalog_value_0%' LIMIT 1").fetchone()[0]
    catalog.run_symbol(f"show {name}")


@when("I show the extracted catalog record without catalog configuration")
def show_known_record_without_configuration(catalog: Catalog) -> None:
    with sqlite3.connect(catalog.context.facts_database_path) as connection:
        name = connection.execute(
            "SELECT qualified_name FROM symbol "
            "WHERE qualified_name='catalog_record'"
        ).fetchone()[0]
    catalog.run_symbol_without_configuration(f"show {name}")


@then("the symbol command succeeds")
def symbol_succeeds(catalog: Catalog) -> None:
    require(catalog.context.last_returncode == 0, catalog.context.last_output)


@then(parsers.parse('the symbol command fails with "{diagnostic}"'))
def symbol_fails(catalog: Catalog, diagnostic: str) -> None:
    require(catalog.context.last_returncode == 1 and
            diagnostic in catalog.context.last_output,
            catalog.context.last_output)


@then("symbol output lists only qualified names and kinds")
def symbol_list_output(catalog: Catalog) -> None:
    lines = catalog.stdout.splitlines()
    header = next((line for line in lines if line.startswith("qualified name")), "")
    symbol = next((line for line in lines if "catalog_value_0" in line), "")
    require(header and symbol and "catalog_value_0" in symbol and
            "function" in symbol and "definition" not in symbol and
            "one.cpp" not in symbol and "id" not in header and
            "flags" not in header and "location" not in header and
            symbol.index("function") == header.index("kind") and
            all("/core/src/one.cpp" not in line for line in lines),
            f"unexpected symbol list columns: {catalog.stdout}")


@then("symbol output contains human-readable function metadata")
def human_readable_function_metadata(catalog: Catalog) -> None:
    expected_source = str(catalog.checkout / "core/src/one.cpp")
    for label in ("catalog_value_0(int amount = 0)", "kind       function",
                  "type       Function", f"source     one.cpp:1:5",
                  expected_source, "properties none", "flags      definition"):
        require(label in catalog.stdout, f"missing {label}: {catalog.stdout}")


@then("symbol output contains human-readable record metadata")
def human_readable_record_metadata(catalog: Catalog) -> None:
    identity = next(line.split()[-1]
                   for line in catalog.stdout.splitlines()
                   if line.startswith("  identity"))
    file_id = identity.split(":", 1)[0]
    for label in ("catalog_record", "kind       struct", "type       Record",
                  f"source     <file {file_id}>:2:8", "properties none",
                  "flags      definition"):
        require(label in catalog.stdout, f"missing {label}: {catalog.stdout}")
    require("properties 0" not in catalog.stdout and
            "implicit" not in catalog.stdout and "static" not in catalog.stdout,
            f"record metadata was not decoded: {catalog.stdout}")


@given("the facts database contains no symbols")
def empty_symbols(catalog: Catalog) -> None:
    with sqlite3.connect(catalog.context.facts_database_path) as connection:
        connection.execute("PRAGMA foreign_keys=ON")
        connection.execute("DELETE FROM symbol")
    catalog.facts_before = catalog.context.facts_database_path.read_bytes()


@then(parsers.parse('the symbol output contains "{value}"'))
def symbol_output(catalog: Catalog, value: str) -> None:
    require(value in catalog.stdout, catalog.context.last_output)
