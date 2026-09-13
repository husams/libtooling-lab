from __future__ import annotations

import json
import os
import re
import shutil
import subprocess
from pathlib import Path

from support.scenario import FactsToolContext

FLOW_FILES = ("variable_flow_root.cpp", "variable_flow_calls.cpp")
COMPLETION = re.compile(r"^facts-tool: variable flow run (\d+) (\S+)$")


def prepare(context: FactsToolContext) -> None:
    """Create an isolated paired project containing both flow translation units."""
    context.prepare()
    root = context.run_root_path
    sources = tuple(root / name for name in FLOW_FILES)
    for name, destination in zip(FLOW_FILES, sources):
        shutil.copy2(context.fixture_root / name, destination)
    context.sources = sources
    commands = [{
        "directory": str(root),
        "file": str(source),
        "arguments": [str(context.compiler), "-std=c++23", "-c", str(source)],
    } for source in sources]
    (root / "compile_commands.json").write_text(
        json.dumps(commands, indent=2) + "\n", encoding="utf-8"
    )
    context.run_import(sources)
    extracted = context.run([
        str(context.facts_tool), "extract", "--output",
        str(context.facts_database_path), "--conf",
        str(context.files_database_path), *(str(source) for source in sources),
    ])
    assert extracted.returncode == 0, extracted.stdout + extracted.stderr
    context.variable_flow_sources = sources
    context.variable_flow_config = root / "variable-flow.yaml"
    context.variable_flow_config.write_text(
        f"conf_template: '{context.files_database_path}'\n"
        f"facts_template: '{context.facts_database_path}'\n",
        encoding="utf-8",
    )


def command(context: FactsToolContext, *args: str, output: Path | None = None,
            config: bool = True, config_path: Path | None = None) -> list[str]:
    argv = [str(context.facts_tool), "analyse", "variable-flow"]
    if config:
        argv.extend(("--config", str(config_path or context.variable_flow_config)))
    else:
        argv.extend(("--conf", str(context.files_database_path)))
    argv.extend(map(str, args))
    if output is not None:
        argv.extend(("--output", str(output)))
    return argv


def run(context: FactsToolContext, *args: str, output: Path | None = None,
        sources: bool = False, config: bool = True,
        config_path: Path | None = None) -> subprocess.CompletedProcess[str]:
    argv = command(context, *args, output=output, config=config,
                   config_path=config_path)
    if sources:
        argv.extend(str(path) for path in context.variable_flow_sources)
    result = subprocess.run(argv, cwd=context.run_root_path, capture_output=True,
                           text=True, check=False, env=_environment(context))
    context.variable_flow_result = result
    context.variable_flow_output = output or context.facts_database_path.with_suffix(
        ".variable-flow.db"
    )
    matches = [COMPLETION.fullmatch(line) for line in result.stdout.splitlines()]
    context.variable_flow_completion = next((match for match in matches if match), None)
    if result.returncode == 0:
        assert context.variable_flow_completion is not None, result.stdout + result.stderr
    return result


def _environment(context: FactsToolContext) -> dict[str, str]:
    environment = dict(os.environ)
    environment.pop("FACTS_TOOL_CONF", None)
    environment.pop("FACTS_TOOL_CONFIG", None)
    environment["XDG_CONFIG_HOME"] = str(context.run_root_path / "no-user-config")
    return environment


def run_id(context: FactsToolContext) -> int:
    assert context.variable_flow_completion is not None
    return int(context.variable_flow_completion.group(1))


def status(context: FactsToolContext) -> str:
    assert context.variable_flow_completion is not None
    return context.variable_flow_completion.group(2)


def read_run(context: FactsToolContext, run_number: int | None = None):
    from facts_tool import open_variable_flow

    selected = run_id(context) if run_number is None else run_number
    with open_variable_flow(context.variable_flow_output) as reader:
        return reader.get(selected)


def read_runs(context: FactsToolContext):
    from facts_tool import open_variable_flow

    with open_variable_flow(context.variable_flow_output) as reader:
        return reader.runs()


def nodes(graph):
    return tuple(graph.nodes)


def edges(graph):
    by_id = {node.id: node for node in nodes(graph)}
    return tuple((by_id[edge.source], by_id[edge.target], edge) for edge in graph.edges)


def kind_matches(value: str, *terms: str) -> bool:
    lowered = value.lower().replace("_", "-")
    return any(term.lower().replace("_", "-") in lowered for term in terms)
