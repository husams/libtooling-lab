"""Real two-component recovery fixture and native command helpers."""
import json
import os
import subprocess

from support.callgraph_run import completion, run_row
from support.callgraph_run_rows import edge_names as _edges
from support.callgraph_run_rows import frontier as _frontier
from support.callgraph_run_rows import recovery as _recovery


def run(context, *args):
    return subprocess.run([str(context.facts_tool), *map(str, args)],
                          env=context.recovery_env, capture_output=True,
                          text=True, timeout=45)


def success(result):
    assert result.returncode == 0, result.stdout + result.stderr
    return result


def prepare(context):
    context.prepare()
    root = context.run_root_path / "recovery"
    root.mkdir()
    context.recovery_env = dict(os.environ, XDG_CONFIG_HOME=str(root / "config"),
                                XDG_DATA_HOME=str(root / "data"))
    context.recovery_sources = []
    bodies = ["int bridge(); int root() { return bridge(); }\n",
              "#ifndef S021_LIBRARY\n#error missing stored macro\n#endif\n"
              '#include "input.hpp"\n'
              "int leaf() { return RECOVERY_VALUE; } int bridge() { return leaf(); }\n"]
    commands = []
    for component, body in zip(("app", "library"), bodies):
        folder = root / component
        folder.mkdir()
        if component == "library":
            (folder / "input.hpp").write_text("#define RECOVERY_VALUE 7\n")
        source = folder / f"{component}.cpp"
        source.write_text(body)
        context.recovery_sources.append(source)
        arguments = [str(context.compiler), "-std=c++23"]
        if component == "library":
            arguments += ["-DS021_LIBRARY=1"]
        commands.append(dict(file=str(source), directory=str(folder),
                             arguments=[*arguments, "-c", str(source)]))
    context.recovery_alternative = root / "app" / "alternative.cpp"
    context.recovery_alternative.write_text("int unused() { return 0; }\n")
    commands.append(dict(file=str(context.recovery_alternative), directory=str(root / "app"),
                         arguments=[str(context.compiler), "-std=c++23", "-c",
                                    str(context.recovery_alternative)]))
    (root / "compile_commands.json").write_text(json.dumps(commands))
    success(run(context, "import", "-v", "0", "--conf", context.files_database_path,
                "--facts", context.facts_database_path, "-p", root,
                "--component", f"app={root / 'app'}",
                "--component", f"library={root / 'library'}"))
    success(extract(context, 0))
    context.recovery_library_body = bodies[1]


def extract(context, position):
    return run(context, "extract", "-v", "0", "--conf", context.files_database_path,
               "--output", context.facts_database_path, context.recovery_sources[position])


def graph(context, recover=True, verbosity=0):
    """Run `analyse call-graph` and return (result, run) from the facts store.

    `run` is None unless the invocation reached the completion line (exit
    0/1/130 with non-empty stdout); otherwise no callgraph_run row exists.
    """
    result = run(context, "analyse", "call-graph", "-v", verbosity, "--conf",
                 context.files_database_path, "--facts", context.facts_database_path,
                 "--function", "root",
                 *(["--recover-missing"] if recover else []))
    info = None
    if result.returncode in (0, 1, 130) and result.stdout.strip():
        run_id, status = completion(result)
        facts = context.facts_database_path
        info = {"run_id": run_id, "status": status, "row": run_row(facts, run_id),
                "edges": _edges(facts, run_id), "recovery": _recovery(facts, run_id),
                "frontier": _frontier(facts, run_id)}
    return result, info


def seed_match(context):
    return success(run(context, "match", "-v", "0", "--conf",
                       context.files_database_path, "--facts", context.facts_database_path,
                       "--matcher", 'functionDecl(hasName("bridge")).bind("symbol")',
                       context.recovery_sources[1]))


def edge_names(run):
    return run["edges"]
