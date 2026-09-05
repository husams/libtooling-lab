import json
import subprocess
import sys
from pathlib import Path


def run(command: list[str]) -> None:
    subprocess.run(command, check=True)


def main() -> None:
    root = Path(__file__).resolve().parent
    facts_tool, compiler = map(Path, sys.argv[1:3])
    source = root / "source.cpp"
    project = root / "project.sqlite"
    facts = root / "facts.sqlite"
    project.unlink(missing_ok=True)
    facts.unlink(missing_ok=True)
    commands = [{
        "directory": str(root),
        "file": str(source),
        "arguments": [str(compiler), "-std=c++20", "-c", str(source)],
    }]
    (root / "compile_commands.json").write_text(json.dumps(commands), encoding="utf-8")
    run([str(facts_tool), "import", "--conf", str(project),
         "--compilation-database", str(root)])
    run([str(facts_tool), "extract", "--conf", str(project),
         "--output", str(facts)])
    (root / "compile_commands.json").unlink()


if __name__ == "__main__":
    main()
