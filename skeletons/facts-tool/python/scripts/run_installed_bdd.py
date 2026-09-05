import os
import subprocess
import sys
import tempfile
from importlib.metadata import version
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def run(*args: str, cwd: Path = ROOT, env: dict[str, str] | None = None) -> None:
    subprocess.run(args, cwd=cwd, env=env, check=True)


def environment_python(environment: Path) -> Path:
    suffix = "Scripts/python.exe" if os.name == "nt" else "bin/python"
    return environment / suffix


def main() -> int:
    with tempfile.TemporaryDirectory(prefix="facts-tool-installed-bdd-") as raw:
        work = Path(raw)
        dist, environment = work / "dist", work / "venv"
        run("uv", "build", "--out-dir", str(dist))
        run("uv", "venv", "--seed", "--python", sys.executable, str(environment))
        python = environment_python(environment)
        wheel = next(dist.glob("*.whl"))
        run(
            str(python),
            "-m",
            "pip",
            "install",
            f"pytest=={version('pytest')}",
            f"pytest-bdd=={version('pytest-bdd')}",
            str(wheel),
        )
        clean_env = os.environ.copy()
        clean_env.pop("PYTHONHOME", None)
        clean_env.pop("PYTHONPATH", None)
        run(
            str(python),
            "-m",
            "pytest",
            str(ROOT / "tests" / "bdd"),
            "-o",
            "addopts=",
            cwd=work,
            env=clean_env,
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
