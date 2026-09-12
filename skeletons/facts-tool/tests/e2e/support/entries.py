from __future__ import annotations

import json
import shutil
import subprocess

from support.callgraph_run import completion, run_row
from support.callgraph_run_rows import edges as run_edges
from support.database import require


def run(context, *arguments, env=None):
    return subprocess.run([str(context.facts_tool), *map(str, arguments)],
                          capture_output=True, text=True, check=False, env=env)


def succeed(result):
    require(result.returncode == 0, result.stdout + result.stderr)
    return result


def prepare(context):
    context.prepare()
    root = context.run_root_path / "entries"
    root.mkdir()
    context.entry_sources = []
    for name, component in (("app.cpp", "app"), ("library.cpp", "library")):
        folder = root / component
        folder.mkdir()
        source = folder / name
        shutil.copy(context.fixture_root / "entries" / name, source)
        shutil.copy(context.fixture_root / "entries" / "api.h", folder / "api.h")
        context.entry_sources.append(source)
    commands = [{"file": str(source), "directory": str(source.parent),
                 "arguments": [str(context.compiler), "-std=c++23", "-c", str(source)]}
                for source in context.entry_sources]
    (root / "compile_commands.json").write_text(json.dumps(commands))
    context.entry_root = root
    import_project(context)


def import_project(context):
    root = context.entry_root
    return succeed(run(context, "import", "-v", "0", "--conf",
                       context.files_database_path, "--facts", context.facts_database_path,
                       "-p", root,
                       "--component", f"app={root / 'app'}",
                       "--component", f"library={root / 'library'}"))


def extract(context, library=False):
    # --force: these scenarios call extract repeatedly against the same
    # source/output to test entry publication and invalidation, not the
    # index-state freshness check, and some of them change nothing the
    # freshness check looks at (e.g. a reimport with the same source mtime).
    return run(context, "extract", "-v", "0", "--force", "--conf",
               context.files_database_path,
               "--output", context.facts_database_path,
               context.entry_sources[1 if library else 0])


def lookup(context, name="left", *, conf=True, facts=None):
    arguments = ["analyse", "call-graph-entry", "-v", "0", "--facts",
                 facts or context.facts_database_path, "--function",
                 name if name.startswith("c:") else f"s027::{name}", "--format", "json"]
    if conf:
        arguments += ["--conf", context.files_database_path]
    result = run(context, *arguments)
    succeed(result)
    return json.loads(result.stdout)


def match(context, expression=None):
    return run(context, "match", "-v", "0", "--conf", context.files_database_path,
               "--facts", context.facts_database_path, "--matcher", expression or
               'functionDecl(hasName("s027::left")).bind("symbol")',
               context.entry_sources[0])


def graph(context, name, *options):
    """Persist an `analyse call-graph` run and return its run row, edges and nodes."""
    root = f"s027::{name}"
    result = succeed(run(context, "analyse", "call-graph", "-v", "0", "--conf",
                         context.files_database_path, "--facts", context.facts_database_path,
                         "--function", root, *options))
    run_id, _ = completion(result)
    facts = context.facts_database_path
    persisted = run_edges(facts, run_id)
    nodes = {root}
    for edge in persisted:
        nodes.add(edge["source"])
        nodes.add(edge["target"])
    return {"run_id": run_id, "row": run_row(facts, run_id), "edges": persisted, "nodes": nodes}
