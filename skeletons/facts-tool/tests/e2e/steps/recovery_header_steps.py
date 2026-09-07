"""A matched header definition resolves through registered TU provenance."""
from pytest_bdd import given
from support.recovery import seed_match


@given("the S-021 match index locates bridge in a registered library header")
def header_definition(context):
    source = context.recovery_sources[1]
    (source.parent / "input.hpp").write_text(
        "#define RECOVERY_VALUE 7\n" +
        context.recovery_library_body.replace('#include "input.hpp"\n', ""))
    source.write_text('#include "input.hpp"\n')
    seed_match(context)
