from __future__ import annotations

import subprocess
import sys
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


if __name__ == "__main__":
    main()
