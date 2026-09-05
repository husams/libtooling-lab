import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).parents[1]


def test_every_example_runs(paired_databases: tuple[Path, Path]) -> None:
    examples = sorted(
        path for path in (ROOT / "examples").glob("*.py") if path.name != "common.py"
    )
    assert examples
    for example in examples:
        completed = subprocess.run(
            [sys.executable, str(example), *map(str, paired_databases)],
            cwd=ROOT,
            capture_output=True,
            text=True,
            check=True,
        )
        for line in completed.stdout.splitlines():
            assert json.loads(line)["provenance"]["pairing"] == "unverifiable"
