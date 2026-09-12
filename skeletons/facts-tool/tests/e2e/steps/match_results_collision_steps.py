from __future__ import annotations

from pathlib import Path

from pytest_bdd import given, parsers, then, when
from support.match_results import import_sources, invoke


@given("an isolated match fixture with colliding relative source paths")
def colliding(context):
    context.prepare()
    commands = []
    context.match_sources = []
    for component, padding, name in [("large", 80000, "large_symbol"),
                                      ("small", 300, "small_symbol")]:
        root = context.run_root_path / component
        (root / "src").mkdir(parents=True)
        (root / "include").mkdir()
        (root / "include/common.hpp").write_text(f"inline int {name}() {{ return 1; }}\n")
        source = root / "src/same.cpp"
        source.write_text('#include "common.hpp"\n//' + "x" * padding + "\n")
        context.match_sources.append(source)
        commands.append({"directory": str(root), "file": "src/same.cpp",
                         "arguments": [str(context.compiler), "-std=c++23",
                                       "-Iinclude", "-c", "src/same.cpp"]})
    import_sources(context, commands)


@when(parsers.parse('structured matching runs in "{order}" order'))
def ordered(context, order):
    sources = context.match_sources if order == "large-small" else list(
        reversed(context.match_sources))
    result = invoke(context,
                    'functionDecl(hasAnyName("large_symbol","small_symbol"))'
                    '.bind("symbol")', sources=sources)
    assert result.returncode == 0, result.stdout + result.stderr


@then("both component-specific symbols and source paths are returned")
def distinct(context):
    from facts_tool import MatchResults

    results = MatchResults.from_json(context.match_completed.stdout)
    assert len(results) == 2
    assert "null character" not in context.match_completed.stderr
    assert {row.bindings["symbol"].name for row in results} == {
        "large_symbol", "small_symbol"}
    for row in results:
        binding = row.bindings["symbol"]
        component = binding.name.split("_")[0]
        assert Path(binding.location.path).resolve() == (
            context.run_root_path / component / "include/common.hpp").resolve()
        assert binding.location.line == 1 and binding.location.column == 12
