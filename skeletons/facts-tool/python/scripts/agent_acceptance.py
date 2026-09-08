from __future__ import annotations

import sys
from pathlib import Path

from agent_sdk import run_sdk


def main() -> None:
    facts, project = map(Path, sys.argv[1:3])
    run_id, setup_calls, setup_chars = map(int, sys.argv[3:6])
    run_sdk(facts, project, run_id, setup_calls, setup_chars)


if __name__ == "__main__":
    main()
