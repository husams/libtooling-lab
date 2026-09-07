"""Real candidates with declaration-only and duplicate definition evidence."""
from pytest_bdd import given
from support.recovery import run, success


@given("S-021 has indexed only the bridge declaration in the app")
def declaration(context):
    success(run(context, "match", "-v", "0", "--conf", context.files_database_path,
                "--facts", context.facts_database_path, "--matcher",
                'functionDecl(hasName("bridge")).bind("symbol")',
                context.recovery_sources[0]))


@given("S-021 bridge is defined in a header included by two registered TUs")
def shared(context):
    header = context.recovery_sources[1].parent / "input.hpp"
    header.write_text("inline int leaf() { return 7; }\n"
                      "inline int bridge() { return leaf(); }\n")
    context.recovery_sources[1].write_text('#include "input.hpp"\n')
    context.recovery_alternative.write_text(f'#include "{header}"\n')
