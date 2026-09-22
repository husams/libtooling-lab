#!/usr/bin/env python3
"""Save a retained job and correlated log records before history is evicted."""

import argparse
from dataclasses import asdict
import json
import os
from pathlib import Path
import time

import yaml
from facts_tool.rest import Client


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("family", choices=("extract", "match", "import", "dependencies",
                                           "callgraphs", "variable-flow", "scan", "index"))
    parser.add_argument("job_id")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    runtime = Path(os.environ.get("FACTS_SERVER_RUNTIME", root / ".server-runtime"))
    config = yaml.safe_load((runtime / "server.yaml").read_text())
    resources = {"extract": "extractions", "match": "matches", "import": "imports",
                 "variable-flow": "variable_flow", "scan": "scans"}
    host = config["host"]
    url = f"http://{'[' + host + ']' if ':' in host else host}:{config['port']}"
    bundle = {"family": args.family, "job_id": args.job_id, "endpoint": url}
    with Client(url, token=os.environ.get("FACTS_TOOL_API_TOKEN"), timeout=60) as api:
        try:
            resource = getattr(api, resources.get(args.family, args.family))
            job = resource.get(args.job_id)
            bundle["metadata"] = asdict(job.metadata)
            bundle["result"] = asdict(job.result) if job.result else None
            if job.result and args.family in {"extract", "match", "dependencies"}:
                bundle["failed_files"] = [asdict(f) for f in resource.failed_files(job.id)]
                bundle["diagnostics"] = [asdict(d) for d in resource.diagnostics(job.id)]
                bundle["records"] = [asdict(row) for row in resource.results(job.id)]
        except Exception as error:
            bundle["lookup_error"] = str(error)
    records = []
    created_at = bundle.get("metadata", {}).get("created_at")
    bundle["log_scope"] = "current job since creation" if created_at is not None else "all retained history matching this ID"
    log = Path(config["logging"]["file"])
    with log.open() as stream:
        for line in stream:
            try:
                record = json.loads(line)
            except json.JSONDecodeError:
                continue
            if (record.get("fields", {}).get("job_id") == args.job_id
                    and (created_at is None or record.get("timestamp", 0) >= created_at)):
                records.append(record)
    bundle["log_records"] = records
    output = args.output or runtime / "evidence" / f"job-{time.time_ns()}.json"
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(bundle, indent=2, default=str) + "\n")
    print(output)
    if "lookup_error" in bundle:
        print("Job is unavailable; matching logs were saved. Job history is lost on restart.")
    return 0 if records or "metadata" in bundle else 1


if __name__ == "__main__":
    raise SystemExit(main())
