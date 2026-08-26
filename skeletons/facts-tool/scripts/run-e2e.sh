#!/usr/bin/env bash
# Run both registered executable E2E gates, retaining CTest and BDD evidence.
# Usage: bash scripts/run-e2e.sh BUILD_DIR [REPORT_DIR]
set -euo pipefail

build_dir="$(cd "${1:?usage: run-e2e.sh BUILD_DIR [REPORT_DIR]}" && pwd)"
report_dir="${2:-$build_dir/e2e-report}"
mkdir -p "$report_dir"
report_dir="$(cd "$report_dir" && pwd)"
test -x "$build_dir/facts-tool"
test -f "$build_dir/CTestTestfile.cmake"

# Do not inherit a developer's -k/-m/--maxfail selection: this is the full gate.
printf -v pytest_arguments -- '--junitxml=%q -o addopts=' "$report_dir/bdd.xml"
status=0
PYTEST_ADDOPTS="$pytest_arguments" ctest --test-dir "$build_dir" \
  --no-tests=error --output-on-failure --output-junit "$report_dir/ctest.xml" \
  -R '^facts-tool-(e2e|cli-contract)$' > "$report_dir/ctest.log" 2>&1 || status=$?
tail -n 16 "$report_dir/ctest.log"
echo "E2E reports: $report_dir"
exit "$status"
