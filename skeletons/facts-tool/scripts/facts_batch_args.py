"""Argument parsing for facts-tool-batch."""

from __future__ import annotations

from dataclasses import dataclass
import argparse
import os


@dataclass(frozen=True)
class BatchArgs:
    mode: str
    jobs: int
    output_dir: str
    conf: str | None
    config: str | None
    extra_args: list[str]
    verbose: int
    files_from: str | None
    compilation_database: str | None
    sources: list[str]


def _positive(value: str) -> int:
    try:
        result = int(value)
    except ValueError as error:
        raise argparse.ArgumentTypeError("must be a positive integer") from error
    if result < 1:
        raise argparse.ArgumentTypeError("must be a positive integer")
    return result


def parse_args(argv: list[str]) -> BatchArgs:
    parser = argparse.ArgumentParser(
        prog="facts-tool-batch",
        description="Run facts-tool once per source with bounded concurrency.",
    )
    subs = parser.add_subparsers(dest="mode", required=True)
    default_jobs = os.cpu_count() or 1
    for mode in ("extract", "dependency"):
        command = subs.add_parser(
            mode, formatter_class=argparse.ArgumentDefaultsHelpFormatter
        )
        command.add_argument(
            "-j",
            "--jobs",
            type=_positive,
            default=default_jobs,
            help="maximum simultaneous facts-tool processes",
        )
        command.add_argument(
            "-o",
            "--output-dir",
            required=True,
            help="directory for per-source databases and logs",
        )
        command.add_argument("-c", "--conf")
        command.add_argument("--config")
        command.add_argument("--extra-arg", action="append", default=[])
        command.add_argument("-v", "--verbose", type=int, choices=range(4), default=0)
        command.add_argument(
            "--files-from", help="source list, one path per line; - for stdin"
        )
        command.add_argument(
            "-p",
            "--compilation-database",
            help="enumerate sources from a compile_commands.json file or directory",
        )
        command.add_argument("sources", nargs="*")
    values = parser.parse_args(argv)
    return BatchArgs(
        values.mode,
        values.jobs,
        values.output_dir,
        values.conf,
        values.config,
        values.extra_arg,
        values.verbose,
        values.files_from,
        values.compilation_database,
        values.sources,
    )
