import os
import subprocess
import sys
from pathlib import Path

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
    tmp_path: Path,
) -> None:
    dist = tmp_path / "dist"
    _run("uv", "build", "--out-dir", str(dist))
    artifacts = sorted((*dist.glob("*.whl"), *dist.glob("*.tar.gz")))
    assert len(artifacts) == 2
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
        _run(
            str(python),
            str(ROOT / "scripts" / "installed_smoke.py"),
            *map(str, paired_databases),
            cwd=tmp_path,
        )
