#!/usr/bin/env python3

from __future__ import annotations

import argparse
from pathlib import Path

from steps import load_step_definitions
from support.bdd import run_features
from support.scenario import FactsToolContext


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--facts-tool", type=Path, required=True)
    parser.add_argument("--fixture-root", type=Path, required=True)
    parser.add_argument("--compiler", type=Path, required=True)
    parser.add_argument("--output-root", type=Path, required=True)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    load_step_definitions()
    run_features(
        Path(__file__).with_name("features"),
        lambda: FactsToolContext.from_args(args),
    )
    print(f"Scenario artifacts root: {args.output_root.resolve()}", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
