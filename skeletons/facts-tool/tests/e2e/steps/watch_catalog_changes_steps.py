"""Watch roots follow public repository commands without restarting the server."""
import subprocess

from pytest_bdd import given, then, when
from support.rest_project import write_commands
from support.watch_observation import refresh, stays_ignored


def checkout(watch_catalog, name):
    root = watch_catalog.server.root / name
    root.mkdir()
    subprocess.run(["git", "init", "-q", str(root)], check=True)
    source = root / "main.cpp"
    source.write_text(f"int {name.replace('-', '_')}_original() {{ return 7; }}\n")
    write_commands(root, watch_catalog.compiler, [source])
    return root, source


@given("a catalog server with an active clone and a registered inactive clone")
def inactive_clone(watch_catalog, compilation=True):
    watch_catalog.create("alpha")
    root, source = checkout(watch_catalog, "secondary-checkout")
    watch_catalog.cli("repo", "add-clone", "alpha", root, "--label", "secondary")
    watch_catalog.inactive, watch_catalog.inactive_source = root, source
    if not compilation:
        (root / "compile_commands.json").unlink()
    watch_catalog.before_identities = watch_catalog.identities()
    watch_catalog.start()


@given("a catalog server whose inactive clone only has stored compilation commands")
def stored_clone(watch_catalog):
    inactive_clone(watch_catalog, compilation=False)


@when("I edit the inactive clone and then the active clone")
def edit_inactive(watch_catalog):
    stays_ignored(watch_catalog, [watch_catalog.inactive_source])
    refresh(watch_catalog, watch_catalog.sources["alpha"][0], "active_updated")


@then("the inactive clone stays inactive and is not imported or extracted")
def inactive_unchanged(watch_catalog):
    assert watch_catalog.identities() == watch_catalog.before_identities
    symbols = watch_catalog.symbols()
    assert "active_updated" in symbols and "excluded_changed" not in symbols


@when("I switch the active clone through the REST repository command")
def switch_clone(watch_catalog):
    watch_catalog.old_source = watch_catalog.sources["alpha"][0]
    watch_catalog.server.api.run(["alpha", "secondary"], "repo/switch")
    watch_catalog.roots["alpha"] = watch_catalog.inactive
    watch_catalog.sources["alpha"] = [watch_catalog.inactive_source]
    watch_catalog.expect_roots(["alpha"])


@when("I edit the former clone and then the newly active clone")
def edit_switched(watch_catalog):
    stays_ignored(watch_catalog, [watch_catalog.old_source])
    refresh(watch_catalog, watch_catalog.inactive_source, "new_active_updated")


@then("only the newly active clone is monitored and its label is preserved")
def switched(watch_catalog):
    symbols = watch_catalog.symbols()
    assert "new_active_updated" in symbols and "excluded_changed" not in symbols
    assert "alpha_original_0" not in symbols, symbols
    active = watch_catalog.rows("SELECT r.name,c.path,c.label FROM repository r "
                                "JOIN clone c ON c.id=r.active_clone_id")
    assert active == [("alpha", str(watch_catalog.inactive), "secondary")]


@when("I register an additional repository through REST while monitoring")
def additional_repository(watch_catalog):
    root, source = checkout(watch_catalog, "third-checkout")
    watch_catalog.server.api.run(["gamma", str(root), "--label", "third"], "repo/add")
    watch_catalog.roots["gamma"], watch_catalog.sources["gamma"] = root, [source]
    watch_catalog.expect_roots(watch_catalog.roots)
    refresh(watch_catalog, source, "gamma_updated")


@then("the added repository is discovered imported and indexed automatically")
def additional_indexed(watch_catalog):
    assert "gamma_updated" in watch_catalog.symbols()
    actual = watch_catalog.rows("SELECT r.name,c.label FROM repository r "
                                "JOIN clone c ON c.id=r.active_clone_id WHERE r.name='gamma'")
    assert actual == [("gamma", "third")]
