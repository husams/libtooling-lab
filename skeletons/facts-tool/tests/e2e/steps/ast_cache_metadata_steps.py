"""Import and dependency commands persist normalized project cache metadata."""
import shutil
import sqlite3

import pytest
from pytest_bdd import given, parsers, then, when

from support.ast_cache_assertions import require_hit, require_miss, require_stored, require_symbol
from support.ast_cache_metadata import (MetadataObservation, downgrade_project, registry_records,
                                        require_dependency_hit, require_dependency_stored,
                                        require_include_fact, require_normalized_inputs, snapshot)
from support.database import query


@pytest.fixture
def cache_metadata():
    return MetadataObservation()


@given("dependency metadata has been imported into the project database")
def imported(ast_cache, cache_metadata):
    ast_cache.run("import")
    require_dependency_stored(ast_cache)
    require_normalized_inputs(ast_cache)
    cache_metadata.rows = snapshot(ast_cache)


@then("import stores normalized input, include, and Git revision rows")
def normalized(ast_cache):
    require_dependency_stored(ast_cache)
    require_normalized_inputs(ast_cache)


@then("import creates neither serialized ASTs nor JSON sidecars")
def no_import_artifacts(ast_cache):
    assert not ast_cache.ast_files()
    assert not tuple(ast_cache.cache.rglob("*.json"))
    assert not query(ast_cache.conf, "SELECT * FROM ast_cache_artifact")


@then("the unchanged dependency metadata is reused without preprocessing")
def unchanged(ast_cache, cache_metadata):
    require_dependency_hit(ast_cache)
    assert snapshot(ast_cache) == cache_metadata.rows


@given("the committed source reports each preprocessing pass")
def preprocessing_probe(ast_cache):
    with ast_cache.source.open("a", encoding="utf-8") as source:
        source.write('\n#pragma message("CACHE_IMPORT_PREPROCESS_PROBE")\n')
    ast_cache.commit_inputs()


@then("import preprocesses the source exactly once while collecting metadata")
def one_preprocessing_pass(ast_cache, cache_metadata):
    ast_cache.succeed()
    diagnostics = ast_cache.last.stderr
    assert diagnostics.count("warning: CACHE_IMPORT_PREPROCESS_PROBE") == 1, diagnostics
    cache_metadata.rows = snapshot(ast_cache)


@then("reimport emits no preprocessing probe warning")
def no_preprocessing_pass(ast_cache):
    assert "warning: CACHE_IMPORT_PREPROCESS_PROBE" not in ast_cache.last.stderr


@when("the source gains an uncommitted preprocessing error and is reimported")
def uncommitted_error(ast_cache):
    with ast_cache.source.open("a", encoding="utf-8") as source:
        source.write("\n#error CACHE_METADATA_MUST_NOT_PREPROCESS\n")
    ast_cache.run("import")


@given(parsers.parse('the serialized AST is "{condition}" but database metadata remains intact'))
def alter_ast(ast_cache, cache_metadata, condition):
    assert ast_cache.ast_files()
    for path in ast_cache.ast_files():
        if condition == "missing":
            path.unlink()
        else:
            assert condition == "corrupt"
            path.write_bytes(b"not a serialized Clang AST")
    cache_metadata.rows = snapshot(ast_cache)
    cache_metadata.artifacts = ast_cache.snapshot_cache()


@then("dependency analysis uses database includes without loading or repairing the AST")
def dependency_without_ast(ast_cache, cache_metadata):
    require_dependency_hit(ast_cache)
    require_include_fact(ast_cache)
    assert snapshot(ast_cache) == cache_metadata.rows
    assert ast_cache.snapshot_cache() == cache_metadata.artifacts


@when("a new source symbol is committed and the project is reimported")
def commit_reimport(ast_cache, cache_metadata):
    cache_metadata.rows = snapshot(ast_cache)
    with ast_cache.source.open("a", encoding="utf-8") as source:
        source.write("\nint cache_metadata_new_commit() { return 42; }\n")
    ast_cache.commit_inputs()
    ast_cache.run("import")


@then("the refreshed dependency snapshot does not validate the previous AST")
def new_generation(ast_cache, cache_metadata):
    require_dependency_stored(ast_cache)
    require_normalized_inputs(ast_cache)
    after = snapshot(ast_cache)
    assert after["ast_cache_snapshot"][0][3] != cache_metadata.rows["ast_cache_snapshot"][0][3]
    assert not after["ast_cache_artifact"], after


@then("extraction regenerates the AST with the newly committed symbol")
def fresh_extraction(ast_cache):
    require_miss(ast_cache)
    require_stored(ast_cache)
    require_symbol(ast_cache, "cache_metadata_new_commit")


@given("malformed legacy JSON sidecars accompany the persisted AST")
def legacy_sidecars(ast_cache, cache_metadata):
    for path in ast_cache.ast_files():
        sidecar = path.with_suffix(".json")
        sidecar.write_bytes(b"{legacy: malformed metadata, do not parse")
    cache_metadata.sidecars = {path: path.read_bytes() for path in ast_cache.cache.rglob("*.json")}


@then("the cached AST is reused and legacy JSON files remain untouched")
def ignore_sidecars(ast_cache, cache_metadata):
    require_hit(ast_cache)
    after = {path: path.read_bytes() for path in ast_cache.cache.rglob("*.json")}
    assert after == cache_metadata.sidecars


@then("the project database contains no persistent cache rows")
def no_metadata(ast_cache):
    ast_cache.succeed()
    assert all(not rows for rows in snapshot(ast_cache).values())
    assert not ast_cache.ast_files()
    assert not tuple(ast_cache.cache.rglob("*.json"))


@given("the AST cache project has no Git repository")
def no_git(ast_cache):
    shutil.rmtree(ast_cache.root / ".git")


@given(parsers.parse("the imported project database has legacy schema version {version:d}"))
def legacy_schema(ast_cache, cache_metadata, version):
    cache_metadata.registry = registry_records(ast_cache)
    downgrade_project(ast_cache, version)


@then("the project schema is migrated while file identities and compiler commands are preserved")
def migrated(ast_cache, cache_metadata):
    ast_cache.succeed()
    assert query(ast_cache.conf, "SELECT schema_version FROM project_registry WHERE id=1") == [(2,)]
    assert registry_records(ast_cache) == cache_metadata.registry
    require_normalized_inputs(ast_cache)


@given("the database artifact digest is damaged")
def damaged_digest(ast_cache):
    with sqlite3.connect(ast_cache.conf) as connection:
        cursor = connection.execute("UPDATE ast_cache_artifact SET digest=?", ("0" * 64,))
        assert cursor.rowcount == 1


@then("extraction replaces the artifact with a valid database digest")
def repaired_digest(ast_cache):
    require_miss(ast_cache)
    require_stored(ast_cache)
    assert query(ast_cache.conf, "SELECT digest FROM ast_cache_artifact") != [("0" * 64,)]
    ast_cache.run("extract")
    require_hit(ast_cache)
