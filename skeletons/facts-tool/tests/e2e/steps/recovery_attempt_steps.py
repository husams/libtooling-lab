"""Include native retry identity/invalidation checks in the complete BDD suite."""
import subprocess
from pytest_bdd import then


@then("S-021 native retry identity and invalidation checks pass")
def retries(context):
    context.prepare()
    executable = context.facts_tool.parent / "recovery-attempts-test"
    result = subprocess.run([str(executable)], cwd=context.run_root_path,
                            capture_output=True, text=True, timeout=30)
    assert result.returncode == 0, result.stdout + result.stderr
