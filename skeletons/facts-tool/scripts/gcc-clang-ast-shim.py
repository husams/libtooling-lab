#!/usr/bin/env python3
"""Add narrowly scoped Clang compatibility flags to GCC compile commands."""

from __future__ import annotations

import argparse
import json
import shlex
from pathlib import Path
from typing import Any


SHIM_RULES: dict[str, tuple[str, ...]] = {
    "/libstdc++-v3/src/c++11/assert_fail.cc": ("-D__noreturn__=",),
    "/libstdc++-v3/src/c++11/random.cc": (
        "-D__builtin_ia32_rdseed_si_step=__builtin_ia32_rdseed32_step",
    ),
}


def shim_arguments(entry: dict[str, Any]) -> list[str]:
    if "arguments" in entry:
        return list(entry["arguments"])
    return shlex.split(entry["command"])


def rules_for(path: str) -> tuple[str, ...]:
    normalized = Path(path).as_posix()
    return next(
        (flags for suffix, flags in SHIM_RULES.items() if normalized.endswith(suffix)),
        (),
    )


def transform(entries: list[dict[str, Any]]) -> tuple[list[dict[str, Any]], list[dict[str, Any]]]:
    transformed: list[dict[str, Any]] = []
    manifest: list[dict[str, Any]] = []
    for original in entries:
        entry = dict(original)
        flags = rules_for(str(entry["file"]))
        if flags:
            arguments = shim_arguments(entry)
            arguments.extend(flag for flag in flags if flag not in arguments)
            if "arguments" in entry:
                entry["arguments"] = arguments
            else:
                entry["command"] = shlex.join(arguments)
            manifest.append({"file": entry["file"], "arguments": list(flags)})
        transformed.append(entry)
    return transformed, manifest


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Create a compile_commands.json with GCC-to-Clang AST shims."
    )
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument(
        "--manifest",
        type=Path,
        help="Optional JSON file recording every transformed translation unit.",
    )
    return parser.parse_args()


def main() -> int:
    options = parse_args()
    entries = json.loads(options.input.read_text())
    transformed, manifest = transform(entries)
    options.output.parent.mkdir(parents=True, exist_ok=True)
    options.output.write_text(json.dumps(transformed, indent=2) + "\n")
    if options.manifest is not None:
        options.manifest.parent.mkdir(parents=True, exist_ok=True)
        options.manifest.write_text(json.dumps(manifest, indent=2) + "\n")
    print(f"shimmed {len(manifest)} of {len(entries)} compile commands")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
