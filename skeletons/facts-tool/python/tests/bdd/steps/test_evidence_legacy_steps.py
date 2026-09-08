import pytest
from pytest_bdd import given, scenarios, then, when

from facts_tool import FactsToolError, open_codebase

scenarios("../features/evidence_legacy.feature")


@given("a legacy evidence pair", target_fixture="legacy_cb")
def legacy_pair(paired_databases):
    with open_codebase(
        facts_db=paired_databases[0], project_db=paired_databases[1]
    ) as cb:
        yield cb


@when("I ask the legacy pair for expressions")
def ask_legacy(legacy_cb, world):
    with pytest.raises(FactsToolError) as error:
        legacy_cb.expression_occurrences()
    world["legacy_error"] = error.value


@then("the legacy capability is rejected")
def check_legacy(world):
    assert world["legacy_error"].code == "E_CAPABILITY"
