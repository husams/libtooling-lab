"""Git ignore policy uses each real checkout's ignore files and tracked index."""
import subprocess

from pytest_bdd import given, parsers, then, when
from support.rest_project import wait_cycle
from support.watch_observation import refresh, settled


@given("an indexed Git repository with nested ignore rules and tracked exceptions")
def git_repository(watch_catalog, yaml=False):
    paths = ["main.cpp", "ignored.cpp", "nested/skip.cpp", "nested/local.cpp",
             "nested/keep.cpp", "tracked.cpp", "generated/keep.cpp"]
    root = watch_catalog.create("alpha", paths)
    (root / ".gitignore").write_text(
        "ignored.cpp\nnested/s*.cpp\nnested/keep.cpp\ntracked.cpp\n"
        "generated/\n!generated/keep.cpp\n")
    (root / "nested/.gitignore").write_text("local.cpp\n!keep.cpp\n")
    subprocess.run(["git", "-C", str(root), "add", "-f", "tracked.cpp"], check=True)
    sources = watch_catalog.sources["alpha"]
    watch_catalog.blocked = [*sources[1:4], sources[6]]
    if yaml:
        watch_catalog.policy["exclude_patterns"] = ["tracked.cpp"]
        watch_catalog.blocked.append(sources[5])
    watch_catalog.before_blocked = watch_catalog.snapshot(watch_catalog.blocked)
    watch_catalog.start()


@given("an indexed Git repository whose tracked source is excluded in YAML")
def tracked_yaml(watch_catalog):
    git_repository(watch_catalog, yaml=True)


@when(parsers.parse('I edit the Git exception "{kind}"'))
def edit_exception(watch_catalog, kind):
    index = {"negated pattern": 4, "tracked file": 5}[kind]
    refresh(watch_catalog, watch_catalog.sources["alpha"][index], "git_exception_updated")


@then("the Git exception is automatically indexed")
def exception_indexed(watch_catalog):
    assert "git_exception_updated" in watch_catalog.symbols()


@when("I remove an ignore rule while the server is running")
def remove_rule(watch_catalog):
    before = settled(watch_catalog)["cycles"]
    path = watch_catalog.roots["alpha"] / ".gitignore"
    path.write_text(path.read_text().replace("ignored.cpp\n", ""))
    wait_cycle(watch_catalog.server, before)
    refresh(watch_catalog, watch_catalog.sources["alpha"][1], "newly_allowed")


@then("the newly allowed source is monitored without a restart")
def newly_allowed(watch_catalog):
    assert "newly_allowed" in watch_catalog.symbols()


@when("I add an ignore rule while the server is running")
def add_rule(watch_catalog):
    before = settled(watch_catalog)["cycles"]
    path = watch_catalog.roots["alpha"] / "nested/.gitignore"
    path.write_text(path.read_text() + "keep.cpp\n")
    wait_cycle(watch_catalog.server, before)
    watch_catalog.blocked = [watch_catalog.sources["alpha"][4]]
    watch_catalog.before_blocked = watch_catalog.snapshot(watch_catalog.blocked)


@given("each repository has a different ignore policy for the same filename")
def independent_git(watch_catalog):
    for name in ("alpha", "beta"):
        root = watch_catalog.create(name, ["main.cpp", "private.cpp"])
        (root / ".gitignore").write_text("private.cpp\n" if name == "alpha" else "")
    watch_catalog.start()


@when("I edit the matching filenames in both clones")
def edit_matching(watch_catalog):
    from support.watch_observation import stays_ignored
    stays_ignored(watch_catalog, [watch_catalog.sources["alpha"][1]])
    refresh(watch_catalog, watch_catalog.sources["beta"][1], "beta_private_updated")


@then("only the clone whose Git policy allows that filename is refreshed")
def independent_result(watch_catalog):
    symbols = watch_catalog.symbols()
    assert "beta_private_updated" in symbols and "excluded_changed" not in symbols
