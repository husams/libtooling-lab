from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path


def run(tool: Path, *arguments: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [str(tool), *arguments], capture_output=True, text=True, check=False
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
        "extract" in output(root_help) and "import" in output(root_help),
        output(root_help),
    )

    extract_help = run(tool, "extract", "--help")
    require(extract_help.returncode == 0, output(extract_help))
    require(
        "--output" in output(extract_help) and "--conf" in output(extract_help),
        output(extract_help),
    )

    import_help = run(tool, "import", "--help")
    require(import_help.returncode == 0, output(import_help))
    require("--compilation-database" in output(import_help), output(import_help))

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
            "project configuration contains no stored compile commands",
        )

        ignored_extra_argument = run(
            tool,
            "import",
            "--conf",
            str(configuration),
            "--compilation-database",
            str(root),
            "--extra-arg=-DIGNORED=1",
        )
        require_failure(
            ignored_extra_argument,
            "--extra-arg cannot be used with --compilation-database",
        )

        first = root / "first.cpp"
        second = root / "second.cpp"
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


if __name__ == "__main__":
    main()
