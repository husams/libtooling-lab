import argparse
from pathlib import Path


def database_paths(description: str) -> tuple[Path, Path]:
    parser = argparse.ArgumentParser(description=description)
    parser.add_argument("facts", type=Path)
    parser.add_argument("project", type=Path)
    args = parser.parse_args()
    return args.facts, args.project
