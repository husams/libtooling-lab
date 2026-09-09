#!/usr/bin/env python3
"""Compare two binaries on one imported TU, with fresh facts databases per run."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import statistics
import subprocess
import time


def arguments():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("baseline", "candidate", "project", "source", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--runs", type=int, default=6,
                        help="measured runs per binary (even balances order)")
    args = parser.parse_args()
    if args.runs < 1:
        parser.error("--runs must be positive")
    for name in ("baseline", "candidate", "project", "source"):
        setattr(args, name, getattr(args, name).resolve(strict=True))
    args.output = args.output.resolve()
    args.output.mkdir(parents=True, exist_ok=False)
    return args


def environment(output):
    env = os.environ.copy()
    for name in ("FACTS_TOOL_CONF", "FACTS_TOOL_CONFIG"):
        env.pop(name, None)
    for variable, folder in (("XDG_CONFIG_HOME", "config"),
                             ("XDG_DATA_HOME", "data")):
        directory = output / folder
        directory.mkdir()
        env[variable] = str(directory)
    env["FACTS_TOOL_TIMING"] = "1"
    return env


def measure(args, label, iteration, env):
    stem = args.output / (label + "-" + str(iteration))
    command = [str(getattr(args, label)), "extract", "-c", str(args.project),
               "-o", str(stem.with_suffix(".db")), "-v", "0", str(args.source)]
    started = time.perf_counter()
    result = subprocess.run(command, cwd=args.output, env=env,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                            text=True)
    elapsed = time.perf_counter() - started
    stem.with_suffix(".log").write_text(result.stderr + result.stdout)
    if result.returncode:
        raise RuntimeError("extraction failed; see " + str(stem.with_suffix(".log")))
    phases = {name: float(value) for name, value in re.findall(
        r"facts-tool timing: (.*): ([\d.e+\-]+) ms", result.stderr)}
    record = {"label": label, "iteration": iteration, "wall_s": elapsed,
              "phases_ms": phases, "command": command,
              "facts": str(stem.with_suffix(".db"))}
    print(label, iteration, format(elapsed, ".4f"), "seconds", flush=True)
    return record


def main():
    args = arguments()
    env = environment(args.output)
    inputs = {name: hashlib.sha256(getattr(args, name).read_bytes()).hexdigest()
              for name in ("baseline", "candidate", "source", "project")}
    runs = []
    # Run zero warms filesystem/library caches and is excluded from medians.
    for iteration in range(args.runs + 1):
        order = ("baseline", "candidate") if iteration % 2 == 0 else (
            "candidate", "baseline")
        runs.extend(measure(args, label, iteration, env) for label in order)
    medians = {label: statistics.median(
        run["wall_s"] for run in runs
        if run["label"] == label and run["iteration"] > 0)
        for label in ("baseline", "candidate")}
    if any(hashlib.sha256(getattr(args, name).read_bytes()).hexdigest() != digest
           for name, digest in inputs.items()):
        raise RuntimeError("benchmark inputs changed during measurement")
    report = {"source": str(args.source), "project": str(args.project),
              "platform": platform.platform(), "machine": platform.machine(),
              "input_sha256": inputs,
              "fresh_facts_per_run": True, "warmups_per_binary": 1,
              "binary_sha256": {label: inputs[label]
                  for label in ("baseline", "candidate")},
              "median_wall_s": medians,
              "reduction_percent": 100 * (1 - medians["candidate"] /
                                           medians["baseline"]), "runs": runs}
    (args.output / "results.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({key: report[key] for key in (
        "median_wall_s", "reduction_percent")}))


if __name__ == "__main__":
    main()
