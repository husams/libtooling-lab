from pytest_bdd import when, then
from steps.external_target_steps import run
from support.database import require


@when("compiler allocation storage invariants are exercised")
def storage(context, implicit_source):
    result = run([str(context.facts_tool.with_name("implicit-storage-test")),
                  str(context.run_root_path / "invariants")])
    context.last_returncode = result.returncode
    context.last_output = result.stdout + result.stderr


@then("all primitive and dynamic storage invariants hold")
def verified(context):
    require(context.last_returncode == 0, context.last_output)
    require("overflow: pass" in context.last_output, context.last_output)
