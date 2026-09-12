import os
import subprocess
import sys
from pathlib import Path

import pytest
from support.callgraph_data import schema12_pair
from support.native_agent import build_native_agent_pair
from support.native_cli_helpers import tool as current_native_tool

ROOT = Path(__file__).parents[1]


def _run(*args: str, cwd: Path = ROOT) -> str:
    environment = os.environ.copy()
    environment.pop("PYTHONHOME", None)
    environment.pop("PYTHONPATH", None)
    result = subprocess.run(
        args,
        cwd=cwd,
        check=True,
        text=True,
        capture_output=True,
        env=environment,
    )
    return result.stdout + result.stderr


def _python(environment: Path) -> Path:
    name = "Scripts/python.exe" if sys.platform == "win32" else "bin/python"
    return environment / name


def test_wheel_and_sdist_install_and_query(
    paired_databases: tuple[Path, Path],
    native_current_pair: tuple[Path, Path],
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    native_tool = current_native_tool()
    assert native_tool.is_file(), "build the current native facts-tool first"
    monkeypatch.setenv("FACTS_TOOL_NATIVE", str(native_tool))
    dist = tmp_path / "dist"
    _run("uv", "build", "--out-dir", str(dist))
    artifacts = sorted((*dist.glob("*.whl"), *dist.glob("*.tar.gz")))
    assert len(artifacts) == 2

    native = build_native_agent_pair(tmp_path / "native-agent")
    for index, artifact in enumerate(artifacts):
        environment = tmp_path / f"clean-{index}"
        _run(
            "uv",
            "venv",
            "--seed",
            "--python",
            sys.executable,
            str(environment),
            cwd=tmp_path,
        )
        python = _python(environment)
        _run(
            str(python),
            "-m",
            "pip",
            "install",
            "--no-deps",
            str(artifact),
            cwd=tmp_path,
        )
        graph_root = tmp_path / f"graph-{index}"
        graph_root.mkdir()
        graph_pair = schema12_pair(*paired_databases, graph_root)
        _run(
            str(python),
            str(ROOT / "scripts" / "installed_smoke.py"),
            *map(str, graph_pair),
            "graph",
            cwd=tmp_path,
        )
        _run(
            str(python),
            str(ROOT / "scripts" / "installed_smoke.py"),
            *map(str, paired_databases),
            cwd=tmp_path,
        )
        transcript = _run(
            str(python),
            str(ROOT / "scripts" / "agent_acceptance.py"),
            str(native.facts),
            str(native.project),
            str(native.run_id),
            str(native.setup_calls),
            str(native.setup_output_chars),
            cwd=tmp_path,
        )
        assert f"callgraphs.get({native.run_id})" in transcript
        assert "class region: exact app::Box" in transcript
        assert "call graph reuse: run ids unchanged" in transcript
        assert "acceptance metrics:" in transcript
        assert "site-packages" in transcript
