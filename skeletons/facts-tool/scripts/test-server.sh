#!/usr/bin/env bash
# Run the shipped API suite in isolated servers, then this instance's acceptance suite.
set -euo pipefail
repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
runtime="$repo_root/.server-runtime"
python="$runtime/venv/bin/python"
run_dir="$runtime/evidence/run-$(date -u +%Y%m%dT%H%M%SZ)"
mkdir -p "$run_dir"
# Several fixtures intentionally have no .git; keep them outside this Git tree.
fixture_root="$(mktemp -d "${TMPDIR:-/tmp}/facts-tool-api.XXXXXX")"
echo "$fixture_root/cases" > "$run_dir/fixtures-location.txt"
status=0
FACTS_TOOL_NATIVE="$runtime/build/facts-tool" "$python" -m pytest \
  "$runtime/source/python/tests/rest" \
  "$runtime/source/python/tests/integration_rest" \
  "$runtime/source/python/tests/test_callgraph_runs.py" \
  "$runtime/source/python/tests/test_variableflow_native.py" \
  "$runtime/source/python/tests/test_variableflow_queries.py" \
  "$runtime/source/python/tests/test_variableflow_reader.py" \
  --junitxml "$run_dir/python.xml" -v > "$run_dir/python.log" 2>&1 || status=1
"$python" -m pytest "$runtime/source/tests/apis" \
  --api-facts-tool "$runtime/build/facts-tool" \
  --basetemp "$fixture_root/cases" --junitxml "$run_dir/api.xml" -v \
  > "$run_dir/api.log" 2>&1 || status=1
"$python" -m pytest "$repo_root/tests/server" \
  --junitxml "$run_dir/instance.xml" -v \
  > "$run_dir/instance.log" 2>&1 || status=1
echo "Results: $run_dir"
tail -n 4 "$run_dir/python.log" "$run_dir/api.log" "$run_dir/instance.log"
exit "$status"
