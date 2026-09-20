"""Compilation database changes must respect the same source exclusion policy."""
from pytest_bdd import then, when
from support.rest_project import wait_cycle, write_commands
from support.watch_observation import settled


@when("the compilation database gains an ignored and an allowed source")
def add_sources(watch_catalog):
    root = watch_catalog.roots["alpha"]
    ignored = root / "generated/never_registered.cpp"
    allowed = root / "new_source.cpp"
    ignored.write_text("int never_registered_symbol() { return 0; }\n")
    allowed.write_text("int newly_registered_symbol() { return 1; }\n")
    before = settled(watch_catalog)["cycles"]
    write_commands(root, watch_catalog.compiler,
                   [*watch_catalog.sources["alpha"], ignored, allowed])
    wait_cycle(watch_catalog.server, before)


@then("only the allowed new source is imported and indexed")
def only_allowed_registered(watch_catalog):
    assert watch_catalog.rows("SELECT name FROM file WHERE name='never_registered.cpp'") == []
    assert watch_catalog.rows("SELECT indexed FROM file WHERE name='new_source.cpp'") == [(1,)]
    symbols = watch_catalog.symbols()
    assert "newly_registered_symbol" in symbols and "never_registered_symbol" not in symbols
