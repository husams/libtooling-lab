"""Configuration and traversal composition evidence for the native workflow."""
from pytest_bdd import when, then, parsers
from support.recovery import run, success
from support.graph_artifact import command, invoke, metadata


@when("S-025 repeats recovery of an already recovered artifact")
def repeat(context):
    success(invoke(context, recover=True))
    success(invoke(context, recover=True, extra=("-v", "1")))


@then("S-025 traverses once and reports no extraction attempts")
def reused(context):
    assert context.graph_result.stderr.count("facts-tool: graph traversal") == 1
    assert metadata(context.graph_artifact)["recovery"]["attempted"] == []


@when("S-025 renders using configured pair defaults")
def configured(context):
    config = context.run_root_path / "graph-config.yaml"
    config.write_text(f"conf_template: '{context.files_database_path}'\n"
                      f"facts_template: '{context.facts_database_path}'\n")
    args = command(context, recover=True)
    for flag in ("--conf", "--facts"):
        index = args.index(flag)
        del args[index:index + 2]
    args += ["--config", str(config)]
    context.graph_result = run(context, *args)


@when(parsers.parse("S-025 renders a budgeted {mode} query"))
def controlled_query(context, mode):
    success(invoke(context, recover=True))
    args = command(context, extra=("--max-nodes", "1"))
    if mode == "callers":
        args[args.index("--function") + 1] = "leaf"
        args += ["--direction", "callers"]
    else:
        args += ["--to", "leaf"]
    context.graph_result = run(context, *args)
    success(context.graph_result)
