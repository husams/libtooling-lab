from __future__ import annotations

import concurrent.futures
import json
import os
import shutil
import sqlite3
import subprocess
import tempfile
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import Optional

from support.database import file_snapshot, require, symbol_snapshot


@dataclass
class FactsToolContext:
    facts_tool: Path
    fixture_root: Path
    compiler: Path
    output_root: Path
    sources: tuple[Path, Path]
    clang_driver: Optional[Path] = None
    run_root: Optional[Path] = None
    facts_database: Optional[Path] = None
    files_database: Optional[Path] = None
    initial_files: list[tuple] = field(default_factory=list)
    initial_symbols: list[tuple] = field(default_factory=list)
    configuration_bytes: Optional[bytes] = None
    last_returncode: Optional[int] = None
    last_output: str = ""
    import_output: str = ""
    prepared: bool = False
    extracted: bool = False
    symlink_target_root: Optional[Path] = None

    @classmethod
    def create(
        cls,
        facts_tool: Path,
        fixture_root: Path,
        compiler: Path,
        output_root: Path,
        clang_driver: Optional[Path] = None,
    ) -> FactsToolContext:
        fixture_root = fixture_root.resolve(strict=True)
        sources = tuple(
            (fixture_root / name).resolve(strict=True)
            for name in ("one.cpp", "two.cpp")
        )
        return cls(
            facts_tool=facts_tool.resolve(strict=True),
            fixture_root=fixture_root,
            compiler=compiler.resolve(strict=True),
            clang_driver=clang_driver.resolve(strict=True) if clang_driver else None,
            output_root=output_root.resolve(),
            sources=(sources[0], sources[1]),
        )

    def prepare(self) -> None:
        if self.prepared:
            return
        self.output_root.mkdir(parents=True, exist_ok=True)
        self.run_root = self.output_root / (f"scenario-{os.getpid()}-{time.time_ns()}")
        self.run_root.mkdir()
        self.facts_database = self.run_root / "facts.sqlite"
        self.files_database = self.run_root / "files.sqlite"
        self._write_compilation_database()
        require(not self.facts_database.exists(), "facts database must start fresh")
        require(not self.files_database.exists(), "files database must start fresh")
        self.prepared = True

    def extract(self) -> None:
        self.prepare()
        if self.extracted:
            return
        self.run_import()
        self.run_tool()
        self.initial_files = file_snapshot(self.files_database_path)
        self.initial_symbols = symbol_snapshot(self.facts_database_path)
        self.extracted = True

    def run_tool(self) -> str:
        completed = self._run(self.tool_command())
        require(
            completed.returncode == 0,
            f"facts-tool exited with {completed.returncode}:\n{self.last_output}",
        )
        require(
            "symbol(s) recorded" in self.last_output,
            f"missing extraction summary:\n{self.last_output}",
        )
        return self.last_output

    def run_import(self, sources: tuple[Path, ...] | None = None) -> str:
        completed = self._run(self.import_command(sources))
        require(
            completed.returncode == 0,
            f"facts-tool import exited with {completed.returncode}:\n{self.last_output}",
        )
        self.import_output = self.last_output
        return self.last_output

    def import_isolated_configuration(self) -> None:
        """Import a project configuration into a run root of its own."""
        self.prepared = False
        self.extracted = False
        self.prepare()
        self.run_import()
        self.configuration_bytes = self.files_database_path.read_bytes()

    def make_configuration_read_only(self) -> None:
        self.files_database_path.chmod(0o444)

    def extract_single_translation_unit(self) -> None:
        try:
            self._run(self._tool_command((self.sources[0],)))
        finally:
            self.files_database_path.chmod(0o644)

    def outdate_file_registry(self) -> None:
        """Leave the registry on a layout only a read-write import can migrate."""
        with sqlite3.connect(self.files_database_path) as connection:
            connection.execute("ALTER TABLE file ADD COLUMN path TEXT")

    def clear_registry_completion(self) -> None:
        """Leave a registry that stores compile commands but claims nothing."""
        with sqlite3.connect(self.files_database_path) as connection:
            connection.execute("UPDATE project_registry SET complete=0")

    def remove_registered_header(self) -> None:
        """Remove one imported header while preserving its source command."""
        with sqlite3.connect(self.files_database_path) as connection:
            row = connection.execute(
                "SELECT id,name FROM file WHERE driver IS NULL AND name='shared.hpp'"
            ).fetchone()
            require(row is not None, "the imported registry contains no shared.hpp")
            connection.execute("DELETE FROM file WHERE id=?", (row[0],))

    def extract_translation_unit(self) -> None:
        self._run(self._tool_command((self.sources[0],)))

    def import_before_the_prefix_header_is_compiled(self) -> None:
        """Import a -include-pch command whose PCH has not been produced yet."""
        self._prepare_prefix_header_project(self.prefix_header_driver)
        self._run(self.import_command((self.prefix_header_source,)))

    def import_and_extract_with_a_compiled_prefix_header(self) -> None:
        driver = self.prefix_header_driver
        self._prepare_prefix_header_project(driver)
        self._compile_prefix_header(driver)
        self.run_import((self.prefix_header_source,))
        self._run(self._tool_command((self.prefix_header_source,)))

    @property
    def prefix_header_driver(self) -> Path:
        """The clang++ that matches the libClang facts-tool links against.

        A precompiled header is only readable by the exact Clang that wrote it,
        so CMAKE_CXX_COMPILER will not do when it is GCC.
        """
        require(
            self.clang_driver is not None,
            "no matching clang++ driver was supplied",
        )
        return self.clang_driver

    @property
    def prefix_header_source(self) -> Path:
        return self.run_root_path / "pch_tu.cpp"

    def _compile_prefix_header(self, driver: Path) -> None:
        completed = subprocess.run(
            [
                str(driver),
                "-std=c++23",
                "-x",
                "c++-header",
                str(self.run_root_path / "pch_prefix.hpp"),
                "-o",
                str(self.run_root_path / "pch_prefix.pch"),
            ],
            capture_output=True,
            text=True,
            check=False,
        )
        require(
            completed.returncode == 0,
            f"cannot compile the prefix header:\n{completed.stderr}",
        )

    def _prepare_prefix_header_project(self, driver: Path) -> None:
        self.prepared = False
        self.extracted = False
        self.prepare()
        (self.run_root_path / "pch_prefix.hpp").write_text(
            "#include <string_view>\n"
            "#include <expected>\n"
            "\n"
            "namespace pch {\n"
            "struct Holder {\n"
            "  std::string_view name;\n"
            "};\n"
            "using Result = std::expected<int, Holder>;\n"
            "Result make() { return 1; }\n"
            "} // namespace pch\n",
            encoding="utf-8",
        )
        self.prefix_header_source.write_text(
            "pch::Result consume() {\n"
            "  auto value = pch::make();\n"
            "  std::string_view text = value ? \"ok\" : value.error().name;\n"
            "  return value;\n"
            "}\n",
            encoding="utf-8",
        )
        require(
            not (self.run_root_path / "pch_prefix.pch").exists(),
            "the prefix header must not be compiled yet",
        )
        self._write_compilation_database(
            sources=(self.prefix_header_source,),
            extra_options={
                self.prefix_header_source.name: [
                    "-include-pch",
                    str(self.run_root_path / "pch_prefix.pch"),
                ]
            },
            driver=driver,
        )

    # --- symlinked generated sources -------------------------------------
    #
    # A compilation database names its sources logically. A generated source
    # that is a symlink into a build mirror still belongs to the project the
    # database describes, so neither the project root nor the file identity may
    # follow the symlink out of the repository.

    @property
    def symlink_project_root(self) -> Path:
        return self.run_root_path / "project"

    @property
    def symlinked_source(self) -> Path:
        return self.symlink_project_root / "generated" / "source.cpp"

    def start_git_rooted_project(self) -> None:
        self.prepared = False
        self.extracted = False
        self.prepare()
        (self.symlink_project_root / "generated").mkdir(parents=True)
        # gitRoot() accepts a .git directory or file; neither needs Git itself.
        (self.symlink_project_root / ".git").mkdir()
        (self.symlink_project_root / "main.cpp").write_text(
            "int main() { return 0; }\n", encoding="utf-8"
        )

    def link_generated_source_outside_the_project(self) -> None:
        """Target a directory that shares no near ancestor with the project.

        The defect only collapses the common root all the way to "/" when the
        two trees diverge at the top, which is what a shadow workspace or a
        /vtmp mirror does in the field.
        """
        self.symlink_target_root = Path(
            tempfile.mkdtemp(prefix="facts-tool-symlink-target-")
        ).resolve()
        target = self.symlink_target_root / "generated_source.cpp"
        target.write_text(
            "int generated_value() { return 41; }\n", encoding="utf-8"
        )
        self.symlinked_source.symlink_to(target)
        require(
            self.symlinked_source.is_symlink(),
            "the generated source must be a symlink",
        )
        require(
            self.symlinked_source.resolve() == target,
            "the symlink must resolve outside the project root",
        )

    def write_symlinked_compilation_database(self) -> None:
        commands = [
            {
                "directory": str(self.symlink_project_root),
                "file": str(self.symlinked_source),
                "arguments": [
                    str(self.compiler),
                    "-std=c++23",
                    "-c",
                    str(self.symlinked_source),
                ],
            }
        ]
        (self.symlink_project_root / "compile_commands.json").write_text(
            json.dumps(commands, indent=2) + "\n", encoding="utf-8"
        )

    def symlinked_import_command(self) -> list[str]:
        return [
            str(self.facts_tool),
            "import",
            "--conf",
            str(self.files_database_path),
            "--compilation-database",
            str(self.symlink_project_root),
        ]

    def import_symlinked_project(self) -> None:
        self._run(self.symlinked_import_command())

    def import_and_extract_symlinked_project(self) -> None:
        completed = self._run(self.symlinked_import_command())
        require(
            completed.returncode == 0,
            f"facts-tool import exited with {completed.returncode}:"
            f"\n{self.last_output}",
        )
        self.import_output = self.last_output
        self._select_facts_database("symlinked-facts.sqlite")
        self._run(self._tool_command((self.symlinked_source,)))

    def discard_symlink_target(self) -> None:
        if self.symlink_target_root is not None:
            shutil.rmtree(self.symlink_target_root, ignore_errors=True)
            self.symlink_target_root = None

    def import_with_unpreprocessable_source(self) -> None:
        self.prepared = False
        self.extracted = False
        self.prepare()
        source = self.run_root_path / "unpreprocessable.cpp"
        source.write_text(
            '#include "definitely_missing_header.hpp"\nint value() { return 1; }\n',
            encoding="utf-8",
        )
        self._write_compilation_database(sources=(source,))
        self._run(self.import_command((source,)))

    def run_concurrently(self) -> list[str]:
        with concurrent.futures.ThreadPoolExecutor(max_workers=2) as executor:
            return list(executor.map(lambda _: self.run_tool(), range(2)))

    def rerun_from_stored_compile_options(self) -> None:
        self.extract()
        self._store_compile_options(self._compile_options())
        self._remove_compilation_database()
        self._select_facts_database("stored-facts.sqlite")
        self._run(self.stored_tool_command())

    def rerun_from_labeled_stored_compile_options(self) -> None:
        self.extract()
        self._store_compile_options(
            [
                "-std=c++23",
                "-I<fixture>",
                "-isystem",
                str(self.fixture_root / "system"),
            ]
        )
        with sqlite3.connect(self.files_database_path) as connection:
            connection.execute(
                "CREATE TABLE label(name TEXT PRIMARY KEY, path TEXT NOT NULL)"
            )
            connection.execute(
                "INSERT INTO label(name,path) VALUES('fixture',?)",
                (str(self.fixture_root),),
            )
        self._remove_compilation_database()
        self._select_facts_database("labeled-stored-facts.sqlite")
        self._run(self.stored_tool_command())

    def rerun_with_json_over_malformed_stored_options(self) -> None:
        self.extract()
        self._set_raw_compile_options('{"invalid":true}')
        self._select_facts_database("json-precedence-facts.sqlite")
        self.run_import()
        self._run(self.tool_command())

    def run_with_malformed_stored_options(self) -> None:
        self.extract()
        self._set_raw_compile_options('{"invalid":true}')
        self._remove_compilation_database()
        self._select_facts_database("malformed-stored-facts.sqlite")
        self._run(self.stored_tool_command())

    def run_with_missing_stored_command(self, filename: str) -> None:
        self.extract()
        self._store_compile_options(self._compile_options())
        with sqlite3.connect(self.files_database_path) as connection:
            connection.execute(
                "UPDATE file SET compile_options=NULL,driver=NULL WHERE name=?",
                (filename,),
            )
        self._remove_compilation_database()
        self._select_facts_database("missing-stored-command-facts.sqlite")
        self._run(self.stored_tool_command())

    def run_with_unrelated_missing_include_root(self) -> None:
        self._run_with_missing_include_root("two.cpp")

    def run_with_missing_include_root_on_selected_source(self) -> None:
        self._run_with_missing_include_root("one.cpp")

    def _run_with_missing_include_root(self, filename: str) -> None:
        """Import a project whose command for `filename` names an absent -I root."""
        self.prepared = False
        self.extracted = False
        self.prepare()
        require(
            not self.missing_include_root.exists(),
            "the missing include root must not exist",
        )
        self._write_compilation_database(
            extra_options={filename: [f"-I{self.missing_include_root}"]}
        )
        self.run_import()
        self._select_facts_database(f"missing-include-root-{filename}.sqlite")
        self._run(self._tool_command((self.sources[0],)))

    @property
    def missing_include_root(self) -> Path:
        return self.run_root_path / "missing-include-root"

    def run_with_deprecated_files_out_option(self) -> None:
        self.prepare()
        self._select_facts_database("deprecated-files-out-facts.sqlite")
        self._run(
            [
                str(self.facts_tool),
                "extract",
                "--output",
                str(self.facts_database_path),
                "--conf",
                str(self.files_database_path),
                "--files-out",
                *(str(source) for source in self.sources),
            ]
        )

    def force_relation_persistence_failure(self) -> None:
        self.extract()
        with sqlite3.connect(self.facts_database_path) as connection:
            connection.execute(
                "CREATE TRIGGER force_relation_failure "
                "BEFORE INSERT ON relation BEGIN "
                "SELECT RAISE(ABORT, 'forced relation persistence failure'); "
                "END"
            )
        self._run(self.tool_command())

    def force_field_relation_persistence_failure(self) -> None:
        self.extract()
        with sqlite3.connect(self.facts_database_path) as connection:
            connection.execute(
                "CREATE TRIGGER force_field_relation_failure "
                "BEFORE INSERT ON relation WHEN NEW.kind=8 BEGIN "
                "SELECT RAISE(ABORT, 'forced field relation persistence failure'); "
                "END"
            )
        self._run(self.tool_command())

    def force_second_inheritance_relation_failure(self) -> None:
        self.extract()
        with sqlite3.connect(self.facts_database_path) as connection:
            connection.execute(
                "CREATE TRIGGER force_second_inheritance_failure "
                "BEFORE INSERT ON relation "
                "WHEN NEW.kind=2 AND NEW.position=1 BEGIN "
                "SELECT RAISE(ABORT, 'forced second inheritance failure'); "
                "END"
            )
        self._run(self.tool_command())

    def run_dependent_base_fixture(self) -> None:
        self.prepare()
        source = (self.fixture_root / "dependent_base.cpp").resolve(strict=True)
        self.facts_database = self.run_root_path / "dependent-facts.sqlite"
        self.files_database = self.run_root_path / "dependent-files.sqlite"
        self._write_compilation_database((source,))
        self.run_import((source,))
        self._run(self._tool_command((source,)))

    def stored_tool_command(self) -> list[str]:
        return [
            str(self.facts_tool),
            "extract",
            "--output",
            str(self.facts_database_path),
            "--conf",
            str(self.files_database_path),
            *(str(source) for source in self.sources),
        ]

    def import_command(self, sources: tuple[Path, ...] | None = None) -> list[str]:
        requested_sources = self.sources if sources is None else sources
        return [
            str(self.facts_tool),
            "import",
            "--conf",
            str(self.files_database_path),
            "--compilation-database",
            str(self.run_root_path),
            *(str(source) for source in requested_sources),
        ]

    def _run(self, command: list[str]) -> subprocess.CompletedProcess[str]:
        completed = subprocess.run(
            command, capture_output=True, text=True, check=False
        )
        self.last_returncode = completed.returncode
        self.last_output = completed.stdout + completed.stderr
        return completed

    def _store_compile_options(self, options: list[str]) -> None:
        self._set_raw_compile_options(json.dumps(options))

    def _compile_options(self) -> list[str]:
        return [
            "-std=c++23",
            f"-I{self.fixture_root}",
            "-isystem",
            str(self.fixture_root / "system"),
        ]

    def _set_raw_compile_options(self, options: str) -> None:
        with sqlite3.connect(self.files_database_path) as connection:
            connection.execute(
                "UPDATE file SET driver=?,compile_options=? "
                "WHERE name IN ('one.cpp','two.cpp')",
                (str(self.compiler), options),
            )

    def _remove_compilation_database(self) -> None:
        (self.run_root_path / "compile_commands.json").unlink()

    def _select_facts_database(self, filename: str) -> None:
        self.facts_database = self.run_root_path / filename

    def tool_command(self) -> list[str]:
        return self._tool_command(self.sources)

    def _tool_command(self, sources: tuple[Path, ...]) -> list[str]:
        return [
            str(self.facts_tool),
            "extract",
            "--output",
            str(self.facts_database_path),
            "--conf",
            str(self.files_database_path),
            *(str(source) for source in sources),
        ]

    @property
    def run_root_path(self) -> Path:
        require(self.run_root is not None, "scenario is not prepared")
        return self.run_root

    @property
    def facts_database_path(self) -> Path:
        require(self.facts_database is not None, "scenario is not prepared")
        return self.facts_database

    @property
    def files_database_path(self) -> Path:
        require(self.files_database is not None, "scenario is not prepared")
        return self.files_database

    def _write_compilation_database(
        self,
        sources: Optional[tuple[Path, ...]] = None,
        extra_options: Optional[dict[str, list[str]]] = None,
        driver: Optional[Path] = None,
    ) -> None:
        selected_sources = sources or self.sources
        options = extra_options or {}
        commands = [
            {
                "directory": str(self.fixture_root),
                "file": str(source),
                "arguments": [
                    str(driver or self.compiler),
                    *self._compile_options(),
                    *options.get(source.name, []),
                    "-c",
                    str(source),
                ],
            }
            for source in selected_sources
        ]
        (self.run_root_path / "compile_commands.json").write_text(
            json.dumps(commands, indent=2) + "\n", encoding="utf-8"
        )
