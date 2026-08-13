#!/usr/bin/env python3

from __future__ import annotations

import argparse
import concurrent.futures
import json
import os
import sqlite3
import subprocess
import time
from collections import defaultdict
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Iterable, Sequence


@dataclass(frozen=True)
class Scenario:
    facts_tool: Path
    fixture_root: Path
    compiler: Path
    run_root: Path
    facts_database: Path
    files_database: Path
    sources: tuple[Path, Path]
    header: Path


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def announce(text: str) -> None:
    print(text, flush=True)


def query(database: Path, sql: str, parameters: Sequence[Any] = ()) -> list[tuple]:
    with sqlite3.connect(database) as connection:
        return connection.execute(sql, parameters).fetchall()


def scalar(database: Path, sql: str, parameters: Sequence[Any] = ()) -> Any:
    rows = query(database, sql, parameters)
    require(len(rows) == 1 and len(rows[0]) == 1, f"not a scalar query: {sql}")
    return rows[0][0]


def table_names(database: Path) -> set[str]:
    return {
        name
        for (name,) in query(
            database,
            "SELECT name FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%'",
        )
    }


def create_fresh_run_root(output_root: Path) -> Path:
    output_root.mkdir(parents=True, exist_ok=True)
    run_root = output_root / f"scenario-{os.getpid()}-{time.time_ns()}"
    run_root.mkdir()
    return run_root


def write_compilation_database(scenario: Scenario) -> None:
    commands = [
        {
            "directory": str(scenario.fixture_root),
            "file": str(source),
            "arguments": [
                str(scenario.compiler),
                "-std=c++23",
                f"-I{scenario.fixture_root}",
                "-c",
                str(source),
            ],
        }
        for source in scenario.sources
    ]
    (scenario.run_root / "compile_commands.json").write_text(
        json.dumps(commands, indent=2) + "\n", encoding="utf-8"
    )


def given_realistic_multifile_cpp(args: argparse.Namespace) -> Scenario:
    facts_tool = args.facts_tool.resolve(strict=True)
    fixture_root = args.fixture_root.resolve(strict=True)
    compiler = args.compiler.resolve(strict=True)
    sources = tuple(
        (fixture_root / name).resolve(strict=True) for name in ("one.cpp", "two.cpp")
    )
    header = (fixture_root / "shared.hpp").resolve(strict=True)
    run_root = create_fresh_run_root(args.output_root.resolve())
    scenario = Scenario(
        facts_tool=facts_tool,
        fixture_root=fixture_root,
        compiler=compiler,
        run_root=run_root,
        facts_database=run_root / "facts.sqlite",
        files_database=run_root / "files.sqlite",
        sources=(sources[0], sources[1]),
        header=header,
    )
    write_compilation_database(scenario)
    require(not scenario.facts_database.exists(), "facts database must start fresh")
    require(not scenario.files_database.exists(), "files database must start fresh")
    return scenario


def tool_command(scenario: Scenario) -> list[str]:
    return [
        str(scenario.facts_tool),
        "-p",
        str(scenario.run_root),
        "--facts-out",
        str(scenario.facts_database),
        "--files-out",
        str(scenario.files_database),
        *(str(source) for source in scenario.sources),
    ]


def run_facts_tool(scenario: Scenario) -> str:
    completed = subprocess.run(
        tool_command(scenario), capture_output=True, text=True, check=False
    )
    output = completed.stdout + completed.stderr
    require(
        completed.returncode == 0,
        f"facts-tool exited with {completed.returncode}:\n{output}",
    )
    require("symbol(s) recorded" in output, f"missing extraction summary:\n{output}")
    return output


def symbol_snapshot(database: Path) -> list[tuple]:
    return query(
        database,
        "SELECT id,file_id,file_index,node,usr,qualified_name "
        "FROM symbol ORDER BY file_id,file_index",
    )


def file_snapshot(database: Path) -> list[tuple]:
    return query(database, "SELECT id,path FROM file ORDER BY id")


def then_paths_are_canonical_and_preimported(scenario: Scenario) -> None:
    files = file_snapshot(scenario.files_database)
    expected_paths = {str(scenario.header), *(str(source) for source in scenario.sources)}
    actual_paths = {path for _, path in files}
    require(actual_paths == expected_paths, f"unexpected file registry: {files}")
    require(all(file_id > 0 for file_id, _ in files), "FileId 0 is reserved")
    require(
        all(Path(path).is_absolute() and Path(path).resolve(strict=True) == Path(path) for _, path in files),
        f"non-canonical source path: {files}",
    )

    imported_ids = {file_id for file_id, _ in files}
    symbol_file_ids = {
        file_id
        for (file_id,) in query(scenario.facts_database, "SELECT DISTINCT file_id FROM symbol")
    }
    require(0 not in symbol_file_ids, "captured symbols must not use builtin FileId 0")
    require(
        symbol_file_ids <= imported_ids,
        f"symbols reference non-preimported FileIds: {symbol_file_ids - imported_ids}",
    )


def then_symbol_ids_are_per_file_and_sequential(scenario: Scenario) -> None:
    by_file: dict[int, list[tuple[int, int]]] = defaultdict(list)
    for packed, file_id, file_index in query(
        scenario.facts_database,
        "SELECT id,file_id,file_index FROM symbol ORDER BY file_id,file_index",
    ):
        by_file[file_id].append((file_index, packed))

    require(by_file, "expected captured symbols")
    for file_id, indexed_symbols in by_file.items():
        indices = [index for index, _ in indexed_symbols]
        require(indices == list(range(len(indices))), f"non-sequential indices for FileId {file_id}: {indices}")
        require(
            all(packed == (file_id << 32) | index for index, packed in indexed_symbols),
            f"invalid packed SymbolId for FileId {file_id}",
        )
        next_index = scalar(
            scenario.facts_database,
            "SELECT next_index FROM symbol_allocator WHERE file_id=?",
            (file_id,),
        )
        require(next_index == len(indices), f"allocator drift for FileId {file_id}")


def rows_by_name(scenario: Scenario) -> dict[str, tuple]:
    return {
        qualified_name: (symbol_id, file_id, file_index, node, usr)
        for symbol_id, file_id, file_index, node, usr, qualified_name in symbol_snapshot(
            scenario.facts_database
        )
    }


def then_repeated_declarations_reuse_usr_identity(scenario: Scenario) -> None:
    expected_repeated = {
        "e2e",
        "e2e::Widget",
        "e2e::headerHelper",
        "e2e::transform",
    }
    symbols = rows_by_name(scenario)
    require(expected_repeated <= symbols.keys(), f"missing repeated declarations: {expected_repeated - symbols.keys()}")
    for name in expected_repeated:
        usr = symbols[name][4]
        require(usr, f"expected a USR for {name}")
        require(
            scalar(scenario.facts_database, "SELECT COUNT(*) FROM symbol WHERE usr=?", (usr,)) == 1,
            f"USR was duplicated for {name}",
        )

    duplicates = query(
        scenario.facts_database,
        "SELECT usr,COUNT(*) FROM symbol WHERE usr<>'' GROUP BY usr HAVING COUNT(*)<>1",
    )
    require(not duplicates, f"duplicate logical symbols: {duplicates}")


def require_node(scenario: Scenario, node: int, expected_names: Iterable[str]) -> None:
    actual = {
        name
        for (name,) in query(
            scenario.facts_database,
            "SELECT qualified_name FROM symbol WHERE node=?",
            (node,),
        )
    }
    expected = set(expected_names)
    require(expected <= actual, f"node {node} missing {expected - actual}; actual={actual}")


def then_concrete_symbols_and_supported_facts_are_present(scenario: Scenario) -> None:
    require_node(
        scenario,
        1,
        {"e2e::headerHelper", "e2e::transform", "e2e::useOne", "e2e::useTwo"},
    )
    require_node(scenario, 2, {"e2e::Widget"})
    require_node(scenario, 3, {"e2e::Mode"})
    require_node(
        scenario,
        4,
        {"e2e::Widget::value", "e2e::Mode::Fast", "e2e::Mode::Slow", "e2e::sharedCounter"},
    )
    require_node(scenario, 5, {"e2e"})
    require_node(scenario, 6, {"e2e::Count"})

    defined = {
        name
        for (name,) in query(
            scenario.facts_database,
            "SELECT s.qualified_name FROM definition d JOIN symbol s ON s.id=d.symbol_id",
        )
    }
    expected_definitions = {
        "e2e::headerHelper",
        "e2e::transform",
        "e2e::useOne",
        "e2e::useTwo",
    }
    require(expected_definitions <= defined, f"missing definitions: {expected_definitions - defined}")

    parameters = query(
        scenario.facts_database,
        "SELECT s.qualified_name,p.position,p.name,p.type,p.has_default "
        "FROM parameter p JOIN symbol s ON s.id=p.symbol_id "
        "ORDER BY s.qualified_name,p.position",
    )
    parameters_by_function: dict[str, list[tuple]] = defaultdict(list)
    for function, position, name, type_name, has_default in parameters:
        parameters_by_function[function].append((position, name, type_name, has_default))
    require(
        [name for _, name, _, _ in parameters_by_function["e2e::transform"]] == ["widget", "factor"],
        f"unexpected transform parameters: {parameters_by_function['e2e::transform']}",
    )
    helper_parameters = parameters_by_function["e2e::headerHelper"]
    require(
        [name for _, name, _, _ in helper_parameters] == ["input", "delta"]
        and helper_parameters[1][3] == 1,
        f"default parameter was not captured: {helper_parameters}",
    )

    require(
        not query(scenario.facts_database, "PRAGMA foreign_key_check"),
        "facts database has broken typed or relation references",
    )
    require(
        not query(
            scenario.facts_database,
            "SELECT r.source_id,r.destination_id FROM relation r "
            "LEFT JOIN symbol s ON s.id=r.source_id "
            "LEFT JOIN symbol d ON d.id=r.destination_id "
            "WHERE s.id IS NULL OR d.id IS NULL",
        ),
        "relation rows must reference captured symbols",
    )


def then_files_and_facts_databases_are_separate(scenario: Scenario) -> None:
    facts_tables = table_names(scenario.facts_database)
    files_tables = table_names(scenario.files_database)
    require("file" not in facts_tables, f"file registry leaked into facts database: {facts_tables}")
    require(files_tables == {"file"}, f"facts leaked into files database: {files_tables}")
    require(
        {"symbol", "symbol_allocator", "definition", "parameter", "relation"} <= facts_tables,
        f"facts schema is incomplete: {facts_tables}",
    )
    require(scenario.facts_database != scenario.files_database, "database paths must differ")


def run_concurrent_processes(scenario: Scenario) -> list[str]:
    with concurrent.futures.ThreadPoolExecutor(max_workers=2) as executor:
        return list(executor.map(lambda _: run_facts_tool(scenario), range(2)))


def then_reruns_and_concurrency_keep_stable_ids(
    scenario: Scenario, initial_files: list[tuple], initial_symbols: list[tuple]
) -> None:
    run_facts_tool(scenario)
    require(file_snapshot(scenario.files_database) == initial_files, "FileIds changed after rerun")
    require(symbol_snapshot(scenario.facts_database) == initial_symbols, "SymbolIds changed after rerun")

    run_concurrent_processes(scenario)
    require(file_snapshot(scenario.files_database) == initial_files, "FileIds changed after concurrency")
    require(
        symbol_snapshot(scenario.facts_database) == initial_symbols,
        "SymbolIds or logical symbols changed after concurrency",
    )
    require(
        scalar(scenario.facts_database, "SELECT COUNT(*) FROM symbol")
        == scalar(scenario.facts_database, "SELECT COUNT(DISTINCT id) FROM symbol"),
        "duplicate SymbolIds were stored",
    )


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--facts-tool", type=Path, required=True)
    parser.add_argument("--fixture-root", type=Path, required=True)
    parser.add_argument("--compiler", type=Path, required=True)
    parser.add_argument("--output-root", type=Path, required=True)
    return parser.parse_args()


def main() -> int:
    announce("Given realistic shared-header declarations, definitions, parameters, and relations")
    scenario = given_realistic_multifile_cpp(parse_args())

    announce("When the real facts-tool indexes both TUs using compile_commands.json")
    run_facts_tool(scenario)
    initial_files = file_snapshot(scenario.files_database)
    initial_symbols = symbol_snapshot(scenario.facts_database)

    announce("Then every source path is canonical and every symbol uses a preimported FileId")
    then_paths_are_canonical_and_preimported(scenario)
    announce("Then SymbolIds are packed from stable, sequential per-file indices")
    then_symbol_ids_are_per_file_and_sequential(scenario)
    announce("Then repeated declarations and translation units reuse each USR's SymbolId")
    then_repeated_declarations_reuse_usr_identity(scenario)
    announce("Then concrete symbol types, definitions, parameters, and supported relations are present")
    then_concrete_symbols_and_supported_facts_are_present(scenario)
    announce("Then file identity and captured facts remain in separate databases")
    then_files_and_facts_databases_are_separate(scenario)

    announce("When indexing is repeated and two extractor processes run concurrently")
    then_reruns_and_concurrency_keep_stable_ids(scenario, initial_files, initial_symbols)
    announce("Then FileIds and SymbolIds remain stable without duplicate logical symbols")
    announce(f"Scenario artifacts: {scenario.run_root}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
