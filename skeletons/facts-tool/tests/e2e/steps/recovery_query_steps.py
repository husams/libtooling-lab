"""Requested recovery failures retain the exit contract across query modes."""
import json
from pytest_bdd import when
from support.recovery import run


def query(context, selector, *options):
    result = run(context, "analyse", "call-graph", "-v", "0", "--conf",
                 context.files_database_path, "--facts", context.facts_database_path,
                 "--format", "json", "--function", selector, "--recover-missing", *options)
    assert result.stdout.strip(), result.stderr
    context.recovery_result = result
    context.recovery_graph = json.loads(result.stdout)


@when("S-021 recovery is requested with a callers query")
def callers(context):
    query(context, "bridge", "--direction", "callers")


@when("S-021 recovery is requested with a path query")
def paths(context):
    query(context, "root", "--to", "bridge")
