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
    context = FactsToolContext.from_args(args)
    run_features(Path(__file__).with_name("features"), context)
    print(f"Scenario artifacts: {context.run_root}", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
