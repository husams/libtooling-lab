"""Requested recovery failures retain the exit contract across query modes."""
from pytest_bdd import when
from support.callgraph_run import completion
from support.callgraph_run_rows import edge_names, recovery
from support.recovery import run


def query(context, selector, *options):
    result = run(context, "analyse", "call-graph", "-v", "0", "--conf",
                 context.files_database_path, "--facts", context.facts_database_path,
                 "--function", selector, "--recover-missing", *options)
    context.recovery_result = result
    context.recovery_run = None
    if result.returncode in (0, 1, 130) and result.stdout.strip():
        run_id, status = completion(result)
        facts = context.facts_database_path
        context.recovery_run = {"run_id": run_id, "status": status,
                                "edges": edge_names(facts, run_id),
                                "recovery": recovery(facts, run_id)}


@when("S-021 recovery is requested with a callers query")
def callers(context):
    query(context, "bridge", "--direction", "callers")


@when("S-021 recovery is requested with a path query")
def paths(context):
    query(context, "root", "--to", "bridge")
