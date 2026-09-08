from dataclasses import FrozenInstanceError

import pytest
from pytest_bdd import given, scenarios, then, when

from facts_tool import open_codebase

scenarios("../features/evidence.feature")


@given("a schema13 evidence pair", target_fixture="evidence_cb")
def evidence_pair(schema13_pair):
    facts, project, _source = schema13_pair
    with open_codebase(facts_db=facts, project_db=project) as cb:
        yield cb


@when("I query expression, field, ancestor, and source evidence")
def query_evidence(evidence_cb, world):
    world["evidence"] = (
        evidence_cb.evidence.expressions("app::run"),
        evidence_cb.evidence.field_writes("app::Box::value"),
        evidence_cb.graph.ancestors("app::Box"),
        evidence_cb.evidence.source_sections("app::run", include_text=True),
    )


@then("evidence results preserve identity, provenance, and immutability")
def check_evidence(world):
    expressions, writes, ancestors, regions = world["evidence"]
    assert (
        expressions.rows[0]["identity"]
        and expressions.provenance.facts.schema.user_version == 13
    )
    assert writes.rows[0]["access"] in {"write", "read_write"}
    assert isinstance(ancestors, list)
    assert regions.rows[0]["text"]
    with pytest.raises(FrozenInstanceError):
        expressions.truncated = True
