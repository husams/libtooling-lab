"""Contract-shape assertions for outcome matrix cases (B-042)."""
import re

from support.callgraph_run import COMPLETION

CONTRACT_LINE = {
    "usage_format": re.compile(r"^facts-tool: usage error: .*--format json$"),
    "usage_output": re.compile(r"^facts-tool: usage error: .*--output x\.mmd$"),
    "usage_unknown_option": re.compile(r"^facts-tool: usage error: .*--bogus-flag$"),
    "usage_missing_root": re.compile(r"^facts-tool: usage error: .*required$"),
    "usage_to_with_all": re.compile(r"^facts-tool: usage error: .*--all$"),
    "usage_invalid_budget": re.compile(r"^facts-tool: usage error: --max-depth:.*$"),
    "configuration_no_project": re.compile(
        r"^facts-tool: configuration error: recovery requires a project configuration$"),
    "database_missing_facts": re.compile(r"^facts-tool: cannot open facts database .*$"),
    "database_empty_all": re.compile(r"^facts-tool: facts database contains no call facts$"),
    "failed_after_traversal": re.compile(r"^facts-tool: no such column: usr.*$"),
    "recovery_failure": re.compile(
        r"^facts-tool: recovery failed for \d+ translation unit\(s\); "
        r"see callgraph_run_recovery run \d+$"),
    "cancel_before_traversal": re.compile(r"^facts-tool: cancelled before traversal$"),
    "cancel_during_recovery": re.compile(
        r"^facts-tool: cancelled; run \d+ keeps the last usable generation$"),
    "commit_failure": re.compile(
        r"^facts-tool: cannot persist call graph run: "
        r"attempt to write a readonly database$"),
}
SUCCESS = {"complete", "truncated"}
COMPLETION_STDOUT = {"recovery_failure", "failed_after_traversal",
                     "cancel_during_recovery"}


def assert_case(outcome, verbosity, result):
    lines = result.stderr.splitlines()
    if outcome == "help":
        assert result.returncode == 0 and result.stdout, result.stdout
        assert not any(line.strip().startswith(("--format", "--output"))
                       for line in result.stdout.splitlines()), result.stdout
        assert result.stderr == "", result.stderr
        return
    if outcome in SUCCESS:
        assert COMPLETION.match(result.stdout.strip()), result.stdout
        if verbosity == 0:
            assert result.stderr == "", result.stderr
        else:
            assert "facts-tool: call-graph: starting" in result.stderr, result.stderr
        return
    pattern = CONTRACT_LINE[outcome]
    if outcome in COMPLETION_STDOUT:
        assert COMPLETION.match(result.stdout.strip()), result.stdout
    else:
        assert result.stdout == "", result.stdout
    matching = [line for line in lines if pattern.match(line)]
    assert len(matching) == 1, f"{outcome}@v{verbosity}: {result.stdout!r} {result.stderr!r}"
    if verbosity == 0:
        assert lines == matching, f"{outcome}@v0 extra stderr: {lines}"
