from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
EXCLUDED_PARTS = {
    ".venv",
    "dist",
    "build",
    "__pycache__",
    ".pytest_cache",
    ".mypy_cache",
    ".ruff_cache",
}
EXCLUDED_NAMES = {"uv.lock"}
SUFFIXES = {".py", ".sql", ".md", ".toml", ".feature"}


def checked_files() -> list[Path]:
    return sorted(
        path
        for path in ROOT.rglob("*")
        if path.is_file()
        and path.suffix in SUFFIXES
        and path.name not in EXCLUDED_NAMES
        and not EXCLUDED_PARTS.intersection(path.parts)
    )


def main() -> int:
    oversized = []
    for path in checked_files():
        count = len(path.read_text(encoding="utf-8").splitlines())
        if count > 100:
            oversized.append(f"{path.relative_to(ROOT)}: {count}")
    if oversized:
        print("files exceed 100 physical lines:\n" + "\n".join(oversized))
        return 1
    print(f"{len(checked_files())} hand-authored files are <=100 lines")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
