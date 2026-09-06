"""Compare ordinary extraction writes and timing using two real CLI builds."""
import argparse
import json
from pathlib import Path
import sqlite3
import statistics
import sys
import time

sys.path.insert(0, str(Path(__file__).parent / "e2e"))
from support.scenario import FactsToolContext
from support.recovery import prepare, extract, success

TABLES = ("callgraph_entry", "callgraph_external_reference",
          "callgraph_unresolved_site", "relation", "relation_site")


def measure(binary, compiler, output):
    context = FactsToolContext.create(
        facts_tool=binary, fixture_root=Path(__file__).parent / "fixtures/e2e",
        compiler=compiler, clang_driver=compiler, output_root=output)
    prepare(context)
    with sqlite3.connect(context.files_database_path) as db:
        for operation in ("INSERT", "UPDATE", "DELETE"):
            db.execute(f"CREATE TRIGGER comparison_index_{operation} BEFORE {operation} "
                       "ON matched_symbol_index BEGIN SELECT RAISE(ABORT, "
                       "'ordinary extract wrote match-only index'); END")
    with sqlite3.connect(context.facts_database_path) as db:
        db.execute("CREATE TABLE comparison_writes (name TEXT, operation TEXT)")
        for table in TABLES:
            for operation in ("INSERT", "UPDATE", "DELETE"):
                db.execute(f"CREATE TRIGGER comparison_{table}_{operation} "
                           f"AFTER {operation} ON {table} BEGIN INSERT INTO "
                           f"comparison_writes VALUES ('{table}','{operation}'); END")
    results = []
    for _ in range(8):
        with sqlite3.connect(context.facts_database_path) as db:
            db.execute("DELETE FROM comparison_writes")
        start = time.perf_counter()
        success(extract(context, 1))
        elapsed = time.perf_counter() - start
        with sqlite3.connect(context.facts_database_path) as db:
            writes = db.execute("SELECT name,operation,count(*) FROM comparison_writes "
                                "GROUP BY name,operation ORDER BY name,operation").fetchall()
            counts = {table: db.execute(f"SELECT count(*) FROM {table}").fetchone()[0]
                      for table in TABLES}
        results.append(dict(seconds=elapsed, writes=writes, rows=counts))
    return dict(binary=str(binary.resolve()), runs=results,
                median_seconds=statistics.median(row["seconds"] for row in results))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--baseline", type=Path, required=True)
    parser.add_argument("--candidate", type=Path, required=True)
    parser.add_argument("--compiler", type=Path, required=True)
    parser.add_argument("--output-root", type=Path, required=True)
    args = parser.parse_args()
    baseline = measure(args.baseline, args.compiler, args.output_root)
    candidate = measure(args.candidate, args.compiler, args.output_root)
    identical = all(a["writes"] == b["writes"] and a["rows"] == b["rows"]
                    for a, b in zip(baseline["runs"], candidate["runs"]))
    print(json.dumps(dict(baseline=baseline, candidate=candidate,
                          identical_graph_writes=identical,
                          matched_index_writes=0), indent=2))
    assert identical, "ordinary extraction graph work differs from baseline"


if __name__ == "__main__":
    main()
