from __future__ import annotations

import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path


def run(
    tool: Path,
    *arguments: str,
    environment: dict[str, str] | None = None,
    working_directory: Path | None = None,
) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [str(tool), *arguments],
        capture_output=True,
        text=True,
        check=False,
        cwd=working_directory,
        env=os.environ | (environment or {}),
    )


def output(result: subprocess.CompletedProcess[str]) -> str:
    return result.stdout + result.stderr


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def require_failure(
    result: subprocess.CompletedProcess[str], expected: str
) -> None:
    require(result.returncode == 1, output(result))
    require(expected in output(result), output(result))


def main() -> None:
    tool = Path(sys.argv[1]).resolve(strict=True)

    root_help = run(tool, "--help")
    require(root_help.returncode == 0, output(root_help))
    require(
        all(command in output(root_help) for command in
            ("extract", "import", "file", "symbol")),
        output(root_help),
    )

    file_help = run(tool, "file", "--help")
    require(file_help.returncode == 0, output(file_help))
    require(all(command in output(file_help) for command in
                ("add", "rm", "remove", "list", "ls", "show",
                 "set-option", "clear-option")), output(file_help))

    file_add_help = run(tool, "file", "add", "--help")
    require(file_add_help.returncode == 0 and
            all(option in output(file_add_help) for option in
                ("--driver", "--working-directory", "--arg")),
            output(file_add_help))

    file_set_help = run(tool, "file", "set-option", "--help")
    require(file_set_help.returncode == 0 and
            "--match" in output(file_set_help) and "--arg" in output(file_set_help),
            output(file_set_help))

    clone_help = run(tool, "repo", "rm-clone", "--help")
    require(clone_help.returncode == 0 and "remove-clone" in output(
        run(tool, "repo", "--help")), output(clone_help))

    symbol_help = run(tool, "symbol", "--help")
    require(symbol_help.returncode == 0 and "--facts" in output(symbol_help) and
            "--conf" not in output(symbol_help) and
            all(command in output(symbol_help) for command in
                ("list", "ls", "show")), output(symbol_help))

    missing_symbol_facts = run(tool, "symbol", "list")
    require(missing_symbol_facts.returncode != 0 and
            "--facts" in output(missing_symbol_facts),
            output(missing_symbol_facts))

    missing_file_conf = run(tool, "file", "list")
    require(missing_file_conf.returncode != 0 and
            "--conf" in output(missing_file_conf), output(missing_file_conf))

    extract_help = run(tool, "extract", "--help")
    require(extract_help.returncode == 0, output(extract_help))
    require(
        "--output" in output(extract_help)
        and "--conf" in output(extract_help)
        and "--extra-arg" in output(extract_help)
        and "repeatable" in output(extract_help)
        and "--verbose" in output(extract_help),
        output(extract_help),
    )

    import_help = run(tool, "import", "--help")
    require(import_help.returncode == 0, output(import_help))
    require(
        "--compilation-database" in output(import_help)
        and "--verbose" in output(import_help),
        output(import_help),
    )
    require(
        "Source files to import; filter compilation database commands or"
        in output(import_help)
        and "use --extra-arg arguments" in output(import_help),
        output(import_help),
    )
    require(
        "Compiler argument appended to fixed-command or"
        in output(import_help)
        and "compile_commands.json imports; repeatable"
        in output(import_help),
        output(import_help),
    )

    dependency_help = run(tool, "analyses", "dependency", "--help")
    require(dependency_help.returncode == 0, output(dependency_help))
    require(
        "--output" in output(dependency_help)
        and "--conf" in output(dependency_help)
        and "--extra-arg" in output(dependency_help)
        and "repeatable" in output(dependency_help)
        and "--verbose" in output(dependency_help),
        output(dependency_help),
    )

    missing = run(tool, "extract")
    require(missing.returncode != 0, output(missing))
    require(
        "--output" in output(missing) and "required" in output(missing).lower(),
        output(missing),
    )

    unknown = run(tool, "unknown")
    require(unknown.returncode != 0, output(unknown))
    require("subcommand" in output(unknown).lower(), output(unknown))

    with tempfile.TemporaryDirectory(prefix="facts-tool-cli-") as temporary:
        root = Path(temporary)
        configuration = root / "project.sqlite"

        malformed_component = run(
            tool,
            "import",
            "--conf",
            str(configuration),
            "--component",
            "malformed",
            "source.cpp",
        )
        require_failure(malformed_component, "--component requires name=path")

        missing_import_source = run(tool, "import", "--conf", str(configuration))
        require_failure(
            missing_import_source,
            "import requires --compilation-database or at least one source",
        )
        require(
            "facts-tool: import: starting" not in output(missing_import_source),
            output(missing_import_source),
        )

        verbose_import = run(
            tool, "import", "-v", "--conf", str(configuration)
        )
        require_failure(
            verbose_import,
            "import requires --compilation-database or at least one source",
        )
        require(
            "facts-tool: import: starting" in verbose_import.stderr
            and "facts-tool: import: parse components" in verbose_import.stderr
            and "compilation_database=" not in verbose_import.stderr
            and "facts-tool: import:" not in verbose_import.stdout,
            output(verbose_import),
        )

        detailed_import = run(
            tool,
            "import",
            "--verbose",
            "2",
            "--conf",
            str(configuration),
        )
        require_failure(
            detailed_import,
            "import requires --compilation-database or at least one source",
        )
        require(
            "compilation_database='fixed commands'" in detailed_import.stderr
            and "requested_sources=0" in detailed_import.stderr
            and "facts-tool: import:" not in detailed_import.stdout,
            output(detailed_import),
        )

        short_detailed_import = run(
            tool,
            "import",
            "-v",
            "2",
            "--conf",
            str(configuration),
        )
        require_failure(
            short_detailed_import,
            "import requires --compilation-database or at least one source",
        )
        require(
            "compilation_database='fixed commands'"
            in short_detailed_import.stderr
            and "requested_sources=0" in short_detailed_import.stderr,
            output(short_detailed_import),
        )

        invalid_verbosity = run(
            tool,
            "extract",
            "--verbose",
            "4",
            "--output",
            str(root / "invalid.sqlite"),
            "--conf",
            str(configuration),
        )
        require(invalid_verbosity.returncode != 0, output(invalid_verbosity))
        require(
            "range" in output(invalid_verbosity).lower(),
            output(invalid_verbosity),
        )

        invalid_import_verbosity = run(
            tool,
            "import",
            "-v",
            "4",
            "--conf",
            str(configuration),
        )
        require(
            invalid_import_verbosity.returncode != 0
            and "range" in output(invalid_import_verbosity).lower(),
            output(invalid_import_verbosity),
        )

        invalid_dependency_verbosity = run(
            tool,
            "analyses",
            "dependency",
            "--verbose",
            "4",
            "--output",
            str(root / "invalid-dependency.sqlite"),
            "--conf",
            str(configuration),
            "source.cpp",
        )
        require(
            invalid_dependency_verbosity.returncode != 0
            and "range" in output(invalid_dependency_verbosity).lower(),
            output(invalid_dependency_verbosity),
        )

        same_database = root / "same.sqlite"
        identical_databases = run(
            tool,
            "extract",
            "--output",
            str(same_database),
            "--conf",
            str(same_database),
        )
        require_failure(
            identical_databases,
            "output and project configuration require separate databases",
        )

        detailed_extract = run(
            tool,
            "extract",
            "--verbose",
            "2",
            "--output",
            str(same_database),
            "--conf",
            str(same_database),
        )
        require_failure(
            detailed_extract,
            "output and project configuration require separate databases",
        )
        require(
            "facts-tool: extract: starting" in detailed_extract.stderr
            and "requested_sources=0" in detailed_extract.stderr
            and "facts-tool: extract: validate database paths"
            in detailed_extract.stderr
            and "facts-tool: extract:" not in detailed_extract.stdout,
            output(detailed_extract),
        )

        short_extract = run(
            tool,
            "extract",
            "-v",
            "--output",
            str(same_database),
            "--conf",
            str(same_database),
        )
        require_failure(
            short_extract,
            "output and project configuration require separate databases",
        )
        require(
            "facts-tool: extract: validate database paths"
            in short_extract.stderr,
            output(short_extract),
        )

        identical_dependency_databases = run(
            tool,
            "analyses",
            "dependency",
            "--output",
            str(same_database),
            "--conf",
            str(same_database),
            "source.cpp",
        )
        require_failure(
            identical_dependency_databases,
            "output and project configuration require separate databases",
        )

        detailed_dependency = run(
            tool,
            "analyses",
            "dependency",
            "--verbose",
            "2",
            "--output",
            str(same_database),
            "--conf",
            str(same_database),
            "source.cpp",
        )
        require_failure(
            detailed_dependency,
            "output and project configuration require separate databases",
        )
        require(
            "facts-tool: dependency: starting" in detailed_dependency.stderr
            and "facts-tool: dependency: validate sources"
            in detailed_dependency.stderr
            and "roots=1" in detailed_dependency.stderr
            and "facts-tool: dependency:" not in detailed_dependency.stdout,
            output(detailed_dependency),
        )

        short_dependency = run(
            tool,
            "analyses",
            "dependency",
            "-v",
            "--output",
            str(same_database),
            "--conf",
            str(same_database),
            "source.cpp",
        )
        require_failure(
            short_dependency,
            "output and project configuration require separate databases",
        )
        require(
            "facts-tool: dependency: validate sources"
            in short_dependency.stderr,
            output(short_dependency),
        )

        compilation_database = root / "compile_commands.json"
        compilation_database.write_text("[]", encoding="utf-8")
        empty_import = run(
            tool,
            "import",
            "--conf",
            str(configuration),
            "--compilation-database",
            str(root),
        )
        require_failure(
            empty_import, "compilation database contains no commands"
        )

        empty_extract = run(
            tool,
            "extract",
            "--output",
            str(root / "facts.sqlite"),
            "--conf",
            str(configuration),
        )
        require_failure(
            empty_extract,
            "project configuration is incomplete",
        )

        first = (root / "first.cpp").resolve()
        second = (root / "second.cpp").resolve()
        # import now discovers and registers files, so the sources must exist.
        first.write_text(
            "struct Base {}; struct Derived : Base {}; "
            "int first() { return 1; }\n",
            encoding="utf-8",
        )
        second.write_text("int second() { return 2; }\n", encoding="utf-8")
        compilation_database.write_text(
            json.dumps(
                [
                    {
                        "directory": str(root),
                        "file": str(source),
                        "arguments": ["clang++", "-std=c++23", str(source)],
                    }
                    for source in (first, second)
                ]
            ),
            encoding="utf-8",
        )

        adjusted_root = (root / "adjusted-project").resolve()
        adjusted_root.mkdir()
        include_root = adjusted_root / "extra-include"
        include_root.mkdir()
        extra_header = (include_root / "extra.hpp").resolve()
        extra_header.write_text("struct ExtraBase {};\n", encoding="utf-8")
        adjusted_source = (adjusted_root / "adjusted.cpp").resolve()
        adjusted_source.write_text(
            '#include "extra.hpp"\nstruct ExtraDerived : ExtraBase {};\n',
            encoding="utf-8",
        )
        original_arguments = [
            "clang",
            "-x",
            "c++",
            "-std=c++23",
            str(adjusted_source),
        ]
        extra_arguments = [f"-I{include_root}", "-DEXTRA_IMPORT=1"]
        adjusted_compilation_database = (
            adjusted_root / "compile_commands.json"
        )
        adjusted_compilation_database.write_text(
            json.dumps(
                [
                    {
                        "directory": str(adjusted_root),
                        "file": str(adjusted_source),
                        "arguments": original_arguments,
                    }
                ]
            ),
            encoding="utf-8",
        )
        adjusted_configuration = root / "adjusted.sqlite"
        adjusted_import = run(
            tool,
            "import",
            "--conf",
            str(adjusted_configuration),
            "--compilation-database",
            str(adjusted_root),
            "--component",
            "adjusted=.",
            *(f"--extra-arg={argument}" for argument in extra_arguments),
        )
        require(adjusted_import.returncode == 0, output(adjusted_import))
        require(
            "Imported 1 compile command(s)" in output(adjusted_import)
            and "Registered 2 file(s)" in output(adjusted_import),
            output(adjusted_import),
        )

        exported = run(
            tool,
            "component",
            "compile-commands",
            "adjusted",
            "--conf",
            str(adjusted_configuration),
        )
        require(exported.returncode == 0, output(exported))
        stored_commands = json.loads(exported.stdout)
        require(len(stored_commands) == 1, output(exported))
        require(
            stored_commands[0]["arguments"]
            == original_arguments + extra_arguments,
            output(exported),
        )

        adjusted_extract = run(
            tool,
            "extract",
            "-v",
            "3",
            "--output",
            str(root / "adjusted-facts.sqlite"),
            "--conf",
            str(adjusted_configuration),
            str(adjusted_source),
        )
        require(adjusted_extract.returncode == 0, output(adjusted_extract))
        require(
            "name='ExtraBase'" in adjusted_extract.stderr
            and "name='ExtraDerived'" in adjusted_extract.stderr,
            output(adjusted_extract),
        )

        runtime_root = (root / "runtime-extra-arguments").resolve()
        runtime_root.mkdir()
        runtime_source = (runtime_root / "runtime.cpp").resolve()
        runtime_source.write_text(
            "#ifndef B021_VALUE\n"
            "#define B021_VALUE 0\n"
            "#endif\n"
            "static_assert(B021_VALUE == 2);\n"
            "struct RuntimeExtraArgument {};\n",
            encoding="utf-8",
        )
        runtime_compilation_database = runtime_root / "compile_commands.json"
        runtime_compilation_database.write_text(
            json.dumps(
                [
                    {
                        "directory": str(runtime_root),
                        "file": str(runtime_source),
                        "arguments": [
                            "clang",
                            "-x",
                            "c++",
                            "-std=c++23",
                            "-DB021_VALUE=0",
                            "-c",
                            str(runtime_source),
                        ],
                    }
                ]
            ),
            encoding="utf-8",
        )
        runtime_configuration = root / "runtime-extra-arguments.sqlite"
        runtime_import = run(
            tool,
            "import",
            "--conf",
            str(runtime_configuration),
            "--compilation-database",
            str(runtime_root),
            "--component",
            "runtime=.",
        )
        require(runtime_import.returncode == 0, output(runtime_import))

        runtime_extra_arguments = ["-DB021_VALUE=1", "-DB021_VALUE=2"]
        runtime_extract = run(
            tool,
            "extract",
            "-v",
            "3",
            "--output",
            str(root / "runtime-extra-arguments-facts.sqlite"),
            "--conf",
            str(runtime_configuration),
            *(f"--extra-arg={argument}" for argument in runtime_extra_arguments),
            str(runtime_source),
        )
        require(runtime_extract.returncode == 0, output(runtime_extract))
        require(
            "name='RuntimeExtraArgument'" in runtime_extract.stderr,
            output(runtime_extract),
        )

        runtime_dependency = run(
            tool,
            "analyses",
            "dependency",
            "--output",
            str(root / "runtime-extra-arguments-dependencies.sqlite"),
            "--conf",
            str(runtime_configuration),
            *(f"--extra-arg={argument}" for argument in runtime_extra_arguments),
            str(runtime_source),
        )
        require(runtime_dependency.returncode == 0, output(runtime_dependency))

        runtime_fixed_configuration = root / "runtime-extra-arguments-fixed.sqlite"
        runtime_fixed_import = run(
            tool,
            "import",
            "--conf",
            str(runtime_fixed_configuration),
            "--component",
            "runtime-fixed=.",
            *(f"--extra-arg={argument}" for argument in runtime_extra_arguments),
            str(runtime_source),
            working_directory=runtime_root,
        )
        require(runtime_fixed_import.returncode == 0, output(runtime_fixed_import))
        runtime_fixed_extract = run(
            tool,
            "extract",
            "--output",
            str(root / "runtime-extra-arguments-fixed-facts.sqlite"),
            "--conf",
            str(runtime_fixed_configuration),
            str(runtime_source),
        )
        require(
            runtime_fixed_extract.returncode == 0, output(runtime_fixed_extract)
        )

        filtered_import = run(
            tool,
            "import",
            "--conf",
            str(root / "filtered.sqlite"),
            "--compilation-database",
            str(root),
            str(first),
        )
        require(filtered_import.returncode == 0, output(filtered_import))
        require(
            "Imported 1 compile command(s)" in output(filtered_import),
            output(filtered_import),
        )

        traced_extract = run(
            tool,
            "extract",
            "-v",
            "3",
            "--output",
            str(root / "traced-facts.sqlite"),
            "--conf",
            str(root / "filtered.sqlite"),
            str(first),
        )
        require(traced_extract.returncode == 0, output(traced_extract))
        require(
            "facts-tool: trace: file resolve requested='" in traced_extract.stderr
            and "facts-tool: trace: file canonical identity='"
            in traced_extract.stderr
            and "facts-tool: trace: ast node kind='CXXRecord' name='Base'"
            in traced_extract.stderr
            and "facts-tool: trace: symbol persisted kind='struct' name='Base'"
            in traced_extract.stderr
            and "facts-tool: trace: relation kind='inherits'"
            in traced_extract.stderr,
            output(traced_extract),
        )

        timed_extract = run(
            tool,
            "extract",
            "--output",
            str(root / "timed-facts.sqlite"),
            "--conf",
            str(root / "filtered.sqlite"),
            str(first),
            environment={"FACTS_TOOL_TIMING": "1"},
        )
        require(timed_extract.returncode == 0, output(timed_extract))
        require(
            "facts-tool timing: open output database:" in timed_extract.stderr
            and "facts-tool timing: extract total:" in timed_extract.stderr
            and "facts-tool: extract: starting" not in timed_extract.stderr,
            output(timed_extract),
        )


if __name__ == "__main__":
    main()
