"""Source list and deterministic output naming."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path


class SourceError(ValueError):
    """A source-list input is invalid."""


def _file(path: Path, label: str) -> Path:
    try:
        result = path.resolve(strict=True)
    except OSError as error:
        raise SourceError(f"{label} does not exist: {path}") from error
    if not result.is_file():
        raise SourceError(f"{label} is not a file: {path}")
    return result


def _read_list(path_text: str, cwd: Path) -> list[Path]:
    if path_text == "-":
        import sys

        lines = sys.stdin.read().splitlines()
    else:
        path = _file(cwd / path_text, "files-from list")
        try:
            lines = path.read_text(encoding="utf-8").splitlines()
        except (OSError, UnicodeError) as error:
            raise SourceError(f"cannot read files-from list {path}: {error}") from error

    return [Path(line) for line in lines if line]


def _compdb(path_text: str, cwd: Path) -> list[Path]:
    candidate = (cwd / path_text).resolve()
    if candidate.is_dir():
        candidate /= "compile_commands.json"
    database = _file(candidate, "compilation database")
    try:
        entries = json.loads(database.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        raise SourceError(
            f"cannot read compilation database {database}: {error}"
        ) from error
    if not isinstance(entries, list):
        raise SourceError(f"compilation database is not an array: {database}")
    result = []
    for index, entry in enumerate(entries):
        if not isinstance(entry, dict) or not isinstance(entry.get("file"), str):
            raise SourceError(
                f"compilation database entry {index} has no file: {database}"
            )
        directory = entry.get("directory", ".")
        if not isinstance(directory, str):
            raise SourceError(
                f"compilation database entry {index} has invalid directory"
            )
        base = (
            Path(directory)
            if Path(directory).is_absolute()
            else database.parent / directory
        )
        result.append(
            base / entry["file"]
            if not Path(entry["file"]).is_absolute()
            else Path(entry["file"])
        )
    return result


def collect_sources(args, cwd: Path) -> list[Path]:
    raw = [Path(source) for source in args.sources]
    if args.files_from:
        raw.extend(_read_list(args.files_from, cwd))
    if args.compilation_database:
        raw.extend(_compdb(args.compilation_database, cwd))
    if not raw:
        raise SourceError("at least one source is required")
    result = []
    seen = set()
    for source in raw:
        path = _file(source if source.is_absolute() else cwd / source, "source")
        if path not in seen:
            seen.add(path)
            result.append(path)
    return result


def output_names(source: Path, output_dir: Path, mode: str) -> tuple[Path, Path]:
    digest = hashlib.sha256(str(source).encode()).hexdigest()
    stem = f"{source.name}-{digest}"
    return output_dir / f"{stem}.db", output_dir / f"{stem}-{mode}.log"
