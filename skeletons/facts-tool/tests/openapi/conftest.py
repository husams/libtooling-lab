"""Generate from isolated copies without modifying tracked source or bindings."""
from pathlib import Path
import shutil
import subprocess
import sys

import pytest


@pytest.fixture
def project():
    return Path(__file__).resolve().parents[2]


@pytest.fixture
def source(project, tmp_path):
    directory = tmp_path / "contract"
    shutil.copytree(project / "src/apis/openapi", directory)
    return directory / "openapi.yaml"


@pytest.fixture
def generate(project):
    def run(*arguments, code=0):
        result = subprocess.run(
            [sys.executable, str(project / "scripts/generate_openapi.py"),
             *map(str, arguments)], capture_output=True, text=True, timeout=30)
        assert result.returncode == code, result.stdout + result.stderr
        return result
    return run


@pytest.fixture
def generated_files():
    def files(root):
        return {str(path.relative_to(root)): path.read_bytes()
                for folder in ("src/apis/generated", "python/src/facts_tool/rest/generated")
                for path in (root / folder).rglob("*") if path.is_file()}
    return files
