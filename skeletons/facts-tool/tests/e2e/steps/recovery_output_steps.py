"""Scratch validation must not claim to publish user facts."""
from pytest_bdd import given, parsers, then
from support.recovery import graph, success
from steps.recovery_cpp_reuse_steps import _variant, recover_and_repeat


@given("the S-021 library combines a vector loop and an indirect call")
def diagnostic_body(context):
    _variant(context, '#pragma message("S021_FRONTEND")\n#include <vector>\n'
             'int value() { return 1; }\n'
             'int bridge() { std::vector<int> v{1,2,3}; int s = 0; '
             'for (int x : v) s += x; int (*call)() = value; '
             'return s + call(); }\n')


@then(parsers.parse("S-021 validation output respects verbosity {level:d}"))
def validation_output(context, level):
    first = success(context.recovery_result)
    recovered = [entry for entry in context.recovery_graph["recovery"]["attempted"]
                 if entry["reason"] == "recovered"]
    assert first.stderr.count("symbol(s) recorded") == len(recovered), first.stderr
    recover_and_repeat(context)
    result, data = graph(context, verbosity=level)
    success(result)
    assert data["recovery"]["attempted"] == [], data
    assert "symbol(s) recorded" not in result.stderr, result.stderr
    message = "recovery-validation: temporary facts only; user facts unchanged"
    if level == 0:
        assert message not in result.stderr, result.stderr
        assert "coverage.unsupported_semantics" not in result.stderr, result.stderr
        assert "facts-tool: trace:" not in result.stderr, result.stderr
    else:
        assert message in result.stderr, result.stderr
        assert "coverage.unsupported_semantics kind=indirect-call" in result.stderr
        if level == 3:
            assert "facts-tool: trace: ast node" in result.stderr, result.stderr
