from __future__ import annotations

import os
import shutil
import sys
from pathlib import Path

from agent_native import run_native
from agent_sdk import run_sdk


def main() -> None:
    native_facts, native_project, schema_facts, schema_project = map(
        Path, sys.argv[1:5]
    )
    tool = os.environ.get("FACTS_TOOL") or shutil.which("facts-tool")
    if tool is None:
        raise RuntimeError("facts-tool executable is required")
    outputs = run_native(tool, native_facts, native_project)
    queries = run_sdk(native_facts, native_project, schema_facts, schema_project)
    chars = sum(len(output) for output in outputs)
    print(
        "agent acceptance passed: "
        f"{len(outputs)} native calls, {queries} SDK queries, "
        f"{chars} output chars, ~{chars // 4} output tokens."
    )


if __name__ == "__main__":
    main()
