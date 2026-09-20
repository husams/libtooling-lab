"""Check the complete generated tree before writing any outputs."""
from pathlib import Path

DIRECTORIES = ("src/apis/generated", "python/src/facts_tool/rest/generated")
PUBLIC_CLIENTS = {f"python/src/facts_tool/rest/{name}.py"
                  for name in ("client", "async_client")}


def emit(output: dict[str, str], root: Path, check: bool) -> bool:
    for name, content in output.items():
        if len(content.splitlines()) > 100:
            raise ValueError(f"Generated file exceeds 100 lines: {name}")
        if name not in PUBLIC_CLIENTS and not any(
                Path(name).is_relative_to(directory) for directory in DIRECTORIES):
            raise ValueError(f"Output outside generated directories: {name}")
    existing = {path.relative_to(root).as_posix() for directory in DIRECTORIES
                for path in (root / directory).rglob("*")
                if path.is_file() and "__pycache__" not in path.parts}
    stale = sorted(existing - output.keys())
    changed = sorted(name for name, content in output.items()
                     if not (root / name).exists()
                     or (root / name).read_text(encoding="utf-8") != content)
    if check:
        for name in changed:
            print(f"Generated file is missing or stale: {name}")
        for name in stale:
            print(f"Unexpected generated file: {name}")
        return not (changed or stale)
    for name in changed:
        path = root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(output[name], encoding="utf-8")
    for name in stale:
        (root / name).unlink()
    return True
