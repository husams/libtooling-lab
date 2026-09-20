"""Unavailable clones recover without blocking healthy repositories or HTTP."""
from pytest_bdd import then, when
from support.rest_http import eventually
from support.rest_project import watch_status
from support.watch_observation import edit, refresh


def outage(watch_catalog):
    state = watch_status(watch_catalog.server)
    notices = str(state["notices"])
    return state if str(watch_catalog.roots["alpha"]) in notices else None


@when("the alpha checkout becomes unavailable and I edit beta")
def unavailable(watch_catalog):
    watch_catalog.offline = watch_catalog.server.root / "offline-alpha"
    watch_catalog.roots["alpha"].rename(watch_catalog.offline)
    eventually(lambda: outage(watch_catalog))
    refresh(watch_catalog, watch_catalog.sources["beta"][0], "beta_during_outage")


@then("watch status reports alpha unavailable while beta and HTTP remain healthy")
def healthy_rest(watch_catalog):
    assert outage(watch_catalog)
    assert "beta_during_outage" in watch_catalog.symbols()
    assert watch_catalog.server.api.request("GET", "/health")[0] == 200


@when("I restore alpha with source edits made while it was unavailable")
def restore(watch_catalog):
    edit(watch_catalog.offline / "main.cpp", "alpha_restored")
    watch_catalog.offline.rename(watch_catalog.roots["alpha"])


@then("monitoring recovers and indexes alpha without a restart or another edit")
def recovered(watch_catalog):
    state = watch_catalog.expect_roots(watch_catalog.roots)
    assert not state["notices"], state
    eventually(lambda: "alpha_restored" in watch_catalog.symbols())
    assert "beta_during_outage" in watch_catalog.symbols()
    assert watch_catalog.identities() == watch_catalog.before_identities


@when("both configured checkouts become unavailable")
def both_unavailable(watch_catalog):
    for name in ("alpha", "beta"):
        target = watch_catalog.server.root / f"offline-{name}"
        watch_catalog.roots[name].rename(target)
        if name == "alpha":
            watch_catalog.offline = target
    def missing():
        state = watch_status(watch_catalog.server)
        return state if all(str(root) in str(state["notices"])
                            for root in watch_catalog.roots.values()) else None
    eventually(missing)


@then("alpha is automatically indexed while beta remains reported unavailable")
def partly_recovered(watch_catalog):
    def alpha_available():
        state = watch_status(watch_catalog.server)
        notices = str(state["notices"])
        return state if (state["directories"] == [str(watch_catalog.roots["alpha"])]
                         and str(watch_catalog.roots["alpha"]) not in notices
                         and str(watch_catalog.roots["beta"]) in notices) else None
    state = eventually(alpha_available)
    assert not state["last_error"], state
    eventually(lambda: "alpha_restored" in watch_catalog.symbols())
    assert watch_catalog.identities() == watch_catalog.before_identities
