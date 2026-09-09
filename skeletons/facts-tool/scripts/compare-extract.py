#!/usr/bin/env python3
"""Compare extracted facts through the public facts_tool SDK, using USR identity."""
import argparse
import hashlib
import json

from facts_tool import Budgets, open_codebase
from facts_tool.queryplan import codebase, nodes, start, view

VIEWS = ("symbol", "parameter", "template_parameter", "template_argument",
         "edge", "site", "definition", "enumeration", "enumerator",
         "initializer", "return_type", "expression_occurrence", "source_region")
SYMBOL_REFERENCES = {"symbol_id", "owner_id", "source_id", "destination_id",
                     "type_id", "receiver_type_id"}
GENERATED_IDENTITY = {"id", "identity", "_db_id", "_key"}


def rows(cb, name):
    result = cb.executor.run((start(codebase()) | view(name) | nodes()).plan)
    metadata = result.to_dict()
    if any(metadata[flag] for flag in ("truncated", "partial", "unknown")):
        raise RuntimeError("incomplete parity evidence for " + name)
    return list(result)


def normalize(row, identities):
    # Packed symbol indices may differ with Clang's pointer traversal order.
    # Preserve all semantic fields, mapping only symbol references to USRs.
    # Retain unmaterialized builtin/unknown type IDs exactly; never drop them.
    return {key: identities.get(value, {"stored_id": value})
            if key in SYMBOL_REFERENCES and value is not None and value != 0
            else value
            for key, value in row.items() if key not in GENERATED_IDENTITY}


def snapshot(facts, project):
    budgets = Budgets(result_cap=1000000, enumeration=1000000)
    with open_codebase(facts_db=facts, project_db=project, budgets=budgets) as cb:
        symbols = rows(cb, "symbol")
        identities = {symbol["id"]: symbol["usr"] for symbol in symbols}
        result = {}
        for name in VIEWS:
            values = symbols if name == "symbol" else rows(cb, name)
            canonical = sorted(json.dumps(normalize(row, identities),
                                          sort_keys=True, separators=(",", ":"))
                               for row in values)
            result[name] = {"count": len(values), "sha256": hashlib.sha256(
                "\n".join(canonical).encode()).hexdigest()}
        return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("baseline", "candidate", "project"):
        parser.add_argument("--" + name, required=True)
    args = parser.parse_args()
    baseline = snapshot(args.baseline, args.project)
    candidate = snapshot(args.candidate, args.project)
    report = {"baseline": baseline, "candidate": candidate,
              "equal": baseline == candidate}
    print(json.dumps(report, indent=2))
    raise SystemExit(0 if report["equal"] else 1)


if __name__ == "__main__":
    main()
