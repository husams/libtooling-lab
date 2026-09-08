import shutil
import sqlite3

from pytest_bdd import given, scenarios, then, when

scenarios("../features/evidence_checkout.feature")


@given("a schema13 checkout pair", target_fixture="checkout_cb")
def checkout_pair(schema13_pair):
    facts, project, source = schema13_pair
    facts_copy = source.parent.parent.parent / "checkout-facts.sqlite"
    project_copy = source.parent.parent.parent / "checkout-project.sqlite"
    shutil.copy2(facts, facts_copy)
    shutil.copy2(project, project_copy)
    alternate = source.parent.parent.parent / "alternate" / "src"
    alternate.mkdir(parents=True)
    shutil.copy2(source, alternate / "main.cpp")
    with sqlite3.connect(project_copy) as db:
        db.execute("UPDATE clone SET path=? WHERE id=1", (str(alternate.parent),))
    from facts_tool import open_codebase

    with open_codebase(facts_db=facts_copy, project_db=project_copy) as cb:
        yield cb


@when("I activate an identical alternate checkout")
def activate_checkout(checkout_cb):
    assert (
        checkout_cb.executor.loader.project.execute("SELECT path FROM clone WHERE id=1")
        .fetchone()[0]
        .endswith("alternate")
    )


@then("source evidence reports checkout unavailability")
def check_checkout(checkout_cb):
    regions = checkout_cb.source_regions("app::run", include_text=True)
    expressions = checkout_cb.expression_occurrences("app::run")
    assert regions.unknown and expressions.unknown
    assert regions.rows[0]["freshness"] == "unavailable"
    assert expressions.rows[0]["freshness"] == "unavailable"
