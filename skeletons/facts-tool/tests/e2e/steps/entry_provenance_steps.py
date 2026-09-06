from pytest_bdd import then

from support.database import require
from support.entries import extract, prepare, run, succeed


@then("S-027 mismatched project and facts stores reject overlapping raw IDs")
def incompatible(context):
    other = type(context).create(facts_tool=context.facts_tool,
        fixture_root=context.fixture_root, compiler=context.compiler,
        clang_driver=context.clang_driver, output_root=context.output_root)
    prepare(other)
    succeed(extract(other))
    before = context.facts_database_path.read_bytes()
    operations = [
        ("analyse", "call-graph-entry", "--function", "s027::left",
         "--facts", context.facts_database_path, "--format", "json"),
        ("extract", "--output", context.facts_database_path, other.entry_sources[0]),
    ]
    for arguments in operations:
        result = run(other, *arguments, "--conf", other.files_database_path, "-v", "0")
        require(result.returncode != 0 and "incompatible-symbol-universe" in result.stderr,
                result.stdout + result.stderr)
        require(context.facts_database_path.read_bytes() == before,
                "incompatible input mutated facts")
