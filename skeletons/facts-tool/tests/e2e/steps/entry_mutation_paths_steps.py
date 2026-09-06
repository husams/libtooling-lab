from pytest_bdd import then

from support.database import require
from support.entries import run


@then("S-027 mutations reject colliding or empty facts paths without writes")
def collisions(context):
    alias = context.entry_root / "conf-alias.sqlite"
    alias.symlink_to(context.files_database_path)
    before = context.files_database_path.read_bytes()
    for command in (["import", "-p", context.entry_root],
                    ["file", "set-option", "--match", "app.cpp$", "--arg=-DS027_BAD=1"]):
        for path in (context.files_database_path, alias, ""):
            result = run(context, *command, "--conf", context.files_database_path,
                         "--facts", path, "-v", "0")
            require(result.returncode != 0, result.stdout + result.stderr)
            require(context.files_database_path.read_bytes() == before,
                    f"project changed after rejected facts path {path}")


@then("S-027 mutations without a known facts pair stop before changing either store")
def unknown_pair(context):
    import json
    import os

    compilation = context.entry_root / "compile_commands.json"
    commands = json.loads(compilation.read_text())
    commands[0]["arguments"].insert(1, "-DS027_UNPAIRED=1")
    compilation.write_text(json.dumps(commands))
    environment = dict(os.environ, XDG_CONFIG_HOME=str(context.entry_root / "no-user-config"))
    for name in ("FACTS_TOOL_CONFIG", "FACTS_TOOL_CONF"):
        environment.pop(name, None)
    project_before = context.files_database_path.read_bytes()
    facts_before = context.facts_database_path.read_bytes()
    mutations = (["import", "-p", context.entry_root],
                 ["file", "set-option", "--match", "app.cpp$", "--arg=-DS027_BAD=1"],
                 ["component", "rm", "--name", "app"],
                 ["repo", "rm", "missing"],
                 ["dir", "rm", "--component", "app", "--path", "."])
    for mutation in mutations:
        result = run(context, *mutation, "--conf", context.files_database_path,
                     "-v", "0", env=environment)
        require(result.returncode != 0, result.stdout + result.stderr)
        require("paired facts store is unknown" in result.stderr and
                "--facts" in result.stderr and "facts_template" in result.stderr,
                result.stdout + result.stderr)
        require(context.files_database_path.read_bytes() == project_before,
                f"project changed after unpaired mutation: {mutation}")
        require(context.facts_database_path.read_bytes() == facts_before,
                f"facts changed after unpaired mutation: {mutation}")
