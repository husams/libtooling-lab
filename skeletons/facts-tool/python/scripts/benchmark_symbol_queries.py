"""Compare indexed lookups/navigation with an optionally selected SDK checkout."""

import argparse
import gc
import json
import sqlite3
import statistics
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk-root", type=Path, default=ROOT / "src")
    parser.add_argument("--symbols", type=int, default=100_000)
    parser.add_argument("--repeat", type=int, default=3)
    args = parser.parse_args()
    sys.path[:0] = [str(args.sdk_root), str(ROOT / "tests")]
    from support.project_data import add_project
    from support.symbol_factory import COLUMNS, symbol

    from facts_tool.neighbors import Neighbors
    from facts_tool.view_loader import ViewLoader
    from facts_tool.view_symbols import lookup_symbol

    schema = (ROOT.parent / "src/storage/Schema.h").read_text()
    schema = schema.split('R"sql(', 1)[1].split(')sql"', 1)[0]
    facts, project = sqlite3.connect(":memory:"), sqlite3.connect(":memory:")
    for db in (facts, project):
        db.row_factory = sqlite3.Row
    facts.executescript(schema)
    project.executescript((ROOT / "tests/fixtures/project_schema.sql").read_text())
    add_project(project, Path("/benchmark/repository"))
    facts.executemany(
        f"INSERT INTO symbol({','.join(COLUMNS)}) "
        f"VALUES({','.join('?' for _ in COLUMNS)})",
        (
            symbol((1 << 32) + i, 1, 13, f"usr{i}", f"app::function{i}")
            for i in range(1, args.symbols + 1)
        ),
    )
    facts.execute(
        "INSERT INTO relation VALUES(?,?,1,0,'none',0,0,0,1)",
        ((1 << 32) + 1, (1 << 32) + args.symbols),
    )
    loader = ViewLoader(facts, project)
    neighbors = Neighbors(facts, loader)
    start = lookup_symbol(facts, loader.files, "app::function1")
    operations = {
        "qualified_name_lookup": lambda: lookup_symbol(
            facts, loader.files, f"app::function{args.symbols}"
        ),
        "one_hop_traversal": lambda: neighbors(start, "calls", False),
    }
    report = {"symbols": args.symbols, "repeats": args.repeat, "seconds": {}}
    for name, operation in operations.items():
        samples, query_counts = [], []
        for _ in range(args.repeat):
            counts = {"facts": 0, "project": 0}

            def record(key, counts=counts):
                def count(_sql):
                    counts[key] += 1

                return count

            facts.set_trace_callback(record("facts"))
            project.set_trace_callback(record("project"))
            gc.collect()
            began = time.perf_counter()
            result = operation()
            samples.append(time.perf_counter() - began)
            assert len(result) == 1
            query_counts.append(counts)
        report["seconds"][name] = {
            "median": statistics.median(samples),
            "samples": samples,
            "sql_statements": query_counts,
        }
    print(json.dumps(report, indent=2))
    facts.close()
    project.close()


if __name__ == "__main__":
    main()
