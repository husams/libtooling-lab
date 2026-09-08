import os
import shutil
import subprocess
import sys
from pathlib import Path

from support.agent_data import add_agent_source_regions
from support.callgraph_data import schema12_pair

ROOT = Path(__file__).parents[1]


def _run(*args: str, cwd: Path = ROOT) -> None:
    environment = os.environ.copy()
    environment.pop("PYTHONHOME", None)
    environment.pop("PYTHONPATH", None)
    subprocess.run(
        args,
        cwd=cwd,
        check=True,
        text=True,
        capture_output=True,
        env=environment,
    )


def _python(environment: Path) -> Path:
    name = "Scripts/python.exe" if sys.platform == "win32" else "bin/python"
    return environment / name


def test_wheel_and_sdist_install_and_query(
    paired_databases: tuple[Path, Path],
    schema13_pair: tuple[Path, Path, Path],
    native_schema12_pair: tuple[Path, Path],
    tmp_path: Path,
) -> None:
    dist = tmp_path / "dist"
    _run("uv", "build", "--out-dir", str(dist))
    artifacts = sorted((*dist.glob("*.whl"), *dist.glob("*.tar.gz")))
    assert len(artifacts) == 2
    for index, artifact in enumerate(artifacts):
        schema_facts = tmp_path / f"agent-schema13-facts-{index}.sqlite"
        schema_project = tmp_path / f"agent-schema13-project-{index}.sqlite"
        shutil.copy2(schema13_pair[0], schema_facts)
        shutil.copy2(schema13_pair[1], schema_project)
        add_agent_source_regions(schema_facts, schema13_pair[2])
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
        _run(
            str(python),
            str(ROOT / "scripts" / "agent_acceptance.py"),
            *map(str, native_schema12_pair),
            str(schema_facts),
            str(schema_project),
            cwd=tmp_path,
        )
        _run(
            str(python),
            str(ROOT / "scripts" / "installed_smoke.py"),
            str(schema_facts),
            str(schema_project),
            "evidence",
            cwd=tmp_path,
        )
