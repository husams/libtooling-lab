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


def native_environment() -> dict[str, str]:
    environment = os.environ.copy()
    for name in ("FACTS_TOOL_NATIVE", "FACTS_CLANGXX"):
        executable = Path(environment.get(name, "")).resolve()
        if not executable.is_file() or not os.access(executable, os.X_OK):
            raise SystemExit(f"Set {name} to an executable to run every BDD scenario")
        environment[name] = str(executable)
    for name in ("PYTHONHOME", "PYTHONPATH", "PYTEST_ADDOPTS"):
        environment.pop(name, None)
    return environment


def main() -> int:
    clean_env = native_environment()
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
            f"{wheel}[rest]",
        )
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
