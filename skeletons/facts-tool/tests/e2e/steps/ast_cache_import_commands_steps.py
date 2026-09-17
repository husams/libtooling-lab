"""Import and consumers must agree on the selected effective compiler command."""
import json

from pytest_bdd import given, parsers, then, when

from support.ast_cache_assertions import require_hit, require_stored, require_symbol
from support.ast_cache_metadata import require_imported_artifact, snapshot
from support.database import query


@given(parsers.parse('duplicate compilation commands with removable flags in "{order}" order'))
def duplicate_commands(ast_cache, order):
    with ast_cache.source.open("a", encoding="utf-8") as source:
        source.write("\n#if CACHE_CHOICE == 1\nint cache_choice_one() { return 1; }\n"
                     "#else\nint cache_choice_two() { return 2; }\n#endif\n")
    ast_cache.commit_inputs()
    commands = []
    for choice in ((1, 2) if order == "winner first" else (2, 1)):
        ast_cache.write_commands("-pipe", f"-DCACHE_CHOICE={choice}")
        commands.extend(json.loads((ast_cache.root / "compile_commands.json").read_text()))
    (ast_cache.root / "compile_commands.json").write_text(json.dumps(commands), encoding="utf-8")
    ast_cache.context_symbol = "cache_choice_one"


@then("import prepares one AST for the deterministic stored command")
def selected_command(ast_cache, cache_metadata):
    require_stored(ast_cache)
    require_imported_artifact(ast_cache)
    assert len(ast_cache.ast_files()) == 1
    options = query(ast_cache.conf, "SELECT compile_options FROM file WHERE name='cache.cpp'")
    assert len(options) == 1
    arguments = json.loads(options[0][0])
    assert "-DCACHE_CHOICE=1" in arguments, arguments
    assert "-DCACHE_CHOICE=2" not in arguments, arguments
    assert "-pipe" not in arguments, arguments
    assert ast_cache.last.stderr.count("ast-cache: stored") == 1, ast_cache.last.stderr
    cache_metadata.rows = snapshot(ast_cache)
    cache_metadata.artifacts = ast_cache.snapshot_cache()


@given("YAML compiler defaults select the cached declaration")
def configured_command(ast_cache):
    with ast_cache.source.open("a", encoding="utf-8") as source:
        source.write("\n#if CACHE_CHOICE == 1\nint cache_choice_one() { return 1; }\n"
                     "#elif CACHE_CHOICE == 2\nint cache_choice_two() { return 2; }\n#endif\n")
    ast_cache.commit_inputs()
    ast_cache.configure(ast_cache=True, extra_args=["-DCACHE_CHOICE=1"])
    ast_cache.context_symbol = "cache_choice_one"


@then("import prepares the configured compiler context")
def prepared_context(ast_cache, cache_metadata):
    require_stored(ast_cache)
    require_imported_artifact(ast_cache)
    cache_metadata.rows = snapshot(ast_cache)
    cache_metadata.artifacts = ast_cache.snapshot_cache()


@when("import overrides the configured compiler declaration")
def override_context(ast_cache):
    with ast_cache.source.open("a", encoding="utf-8") as source:
        source.write("\n#if CACHE_CHOICE != 2\n#error CACHE_IMPORT_OVERRIDE_IGNORED\n#endif\n")
    ast_cache.commit_inputs()
    ast_cache.run("import", "--extra-arg=-DCACHE_CHOICE=2")
    ast_cache.context_symbol = "cache_choice_two"


@then("import prepares the overridden compiler context once")
def prepared_override(ast_cache, cache_metadata):
    prepared_context(ast_cache, cache_metadata)
    options = query(ast_cache.conf, "SELECT compile_options FROM file WHERE name='cache.cpp'")
    assert len(options) == 1, options
    arguments = json.loads(options[0][0])
    assert arguments.count("-DCACHE_CHOICE=2") == 1, arguments
    assert "-DCACHE_CHOICE=1" not in arguments, arguments


@given("a standalone compiler forced header outside the source and include search roots")
def standalone_forced_header(ast_cache):
    header = ast_cache.root.parent / "cache_standalone_forced.hpp"
    header.write_text("inline int cache_forced_standalone() { return 42; }\n", encoding="utf-8")
    ast_cache.write_commands("-include", str(header))
    ast_cache.context_symbol = "cache_forced_standalone"
    ast_cache.standalone_forced_header = header


@then("the standalone forced header has a registered file identity")
def registered_forced_header(ast_cache):
    assert len(query(ast_cache.conf, "SELECT id FROM file WHERE name='cache_standalone_forced.hpp'")) == 1
    inputs = snapshot(ast_cache)["ast_cache_input"]
    assert str(ast_cache.standalone_forced_header) in {path for _, path in inputs}, inputs


@when(parsers.parse('the first imported-context "{family}" runs'))
def first_consumer(ast_cache, family):
    command = ast_cache.command(family)
    if family == "match":
        index = command.index("--matcher") + 1
        command[index] = f'functionDecl(hasName("{ast_cache.context_symbol}")).bind("symbol")'
    ast_cache.run(family, command=command)


@then("the imported compiler context is reused with its selected declaration")
def selected_declaration(ast_cache, cache_metadata):
    require_hit(ast_cache)
    require_symbol(ast_cache, ast_cache.context_symbol)
    assert "dependency-cache: miss" not in ast_cache.last.stderr, ast_cache.last.stderr
    assert snapshot(ast_cache) == cache_metadata.rows
    assert ast_cache.snapshot_cache() == cache_metadata.artifacts
