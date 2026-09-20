"""Database-driven roots, excluded work, and catalog identities."""
from pytest_bdd import then, when
from support.watch_observation import refresh, stays_ignored


@when("I edit sources in both configured repositories")
def edit_both(watch_catalog):
    for name in ("alpha", "beta"):
        refresh(watch_catalog, watch_catalog.sources[name][0], f"{name}_updated")


@then("both repositories publish fresh symbols without changing catalog identities")
def both_updated(watch_catalog):
    symbols = watch_catalog.symbols()
    assert "alpha_updated" in symbols and "beta_updated" in symbols, symbols
    assert watch_catalog.identities() == watch_catalog.before_identities


@when("I edit beta and then alpha")
def edit_excluded_repository(watch_catalog):
    watch_catalog.blocked = watch_catalog.sources["beta"]
    watch_catalog.before_blocked = watch_catalog.snapshot(watch_catalog.blocked)
    stays_ignored(watch_catalog, watch_catalog.blocked)
    refresh(watch_catalog, watch_catalog.sources["alpha"][0], "alpha_updated")


@then("only alpha is monitored and beta keeps its imported and indexed state")
def only_alpha(watch_catalog):
    watch_catalog.expect_roots(["alpha"])
    unchanged_exclusions(watch_catalog)
    assert watch_catalog.identities() == watch_catalog.before_identities


@when("I edit the excluded sources and then an allowed source")
def edit_excluded_sources(watch_catalog):
    stays_ignored(watch_catalog, watch_catalog.blocked)
    refresh(watch_catalog, watch_catalog.sources["alpha"][0], "alpha_updated")


@then("excluded sources are neither reimported nor reindexed")
def unchanged_exclusions(watch_catalog):
    assert watch_catalog.snapshot(watch_catalog.blocked) == watch_catalog.before_blocked
    symbols = watch_catalog.symbols()
    assert "alpha_updated" in symbols, symbols
    assert "excluded_changed" not in symbols, symbols


@then("monitoring has not created Git metadata in the source directory")
def no_git_created(watch_catalog):
    assert not (watch_catalog.roots["alpha"] / ".git").exists()
