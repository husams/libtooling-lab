from __future__ import annotations

import json
import subprocess

from support.scenario import FactsToolContext


MATCHERS = {
    "symbol": 'functionDecl(hasName("shared_match"),isDefinition()).bind("symbol")',
    "call": ('callExpr(callee(functionDecl(hasName("shared_match"))'
             '.bind("callee"))).bind("call")'),
    "expression": ('callExpr(callee(functionDecl(hasName("shared_match"))))'
                   '.bind("expression")'),
    "relation": ('cxxRecordDecl(hasName("MatchRecord"),isDefinition(),'
                 'isDerivedFrom(cxxRecordDecl(hasName("MatchBase"))'
                 '.bind("target"))).bind("source")'),
    "empty": 'functionDecl(hasName("absent_match_9621")).bind("symbol")',
}


def invoke(context: FactsToolContext, matcher: str, *, text: bool = False,
           sources=None, relation: bool = False):
    command = [str(context.facts_tool), "match", "--conf",
               str(context.files_database), "--facts", str(context.facts_database),
               "--matcher", matcher, "--format", "text" if text else "json"]
    if relation:
        command += ["--relation-kind", relation if isinstance(relation, str) else "Inherits"]
    command += [str(p) for p in (sources or context.match_sources)]
    context.match_completed = subprocess.run(
        command, capture_output=True, text=True, cwd=context.run_root_path)
    return context.match_completed


def import_sources(context: FactsToolContext, commands=None):
    if commands is None:
        commands = [{"directory": str(context.run_root_path), "file": str(path),
                     "arguments": [str(context.compiler), "-std=c++23", "-c",
                                   str(path)]} for path in context.match_sources]
    (context.run_root_path / "compile_commands.json").write_text(json.dumps(commands))
    result = subprocess.run(
        [str(context.facts_tool), "import", "--conf", str(context.files_database),
         "--facts", str(context.facts_database), "-p", str(context.run_root_path)],
        capture_output=True, text=True, cwd=context.run_root_path)
    assert result.returncode == 0, result.stdout + result.stderr


def symbol_snapshot(context: FactsToolContext, name: str):
    from facts_tool import open_codebase

    with open_codebase(facts_db=context.facts_database,
                       project_db=context.files_database) as cb:
        entity = cb.find(name)
        return None if entity is None else entity.to_dict()
