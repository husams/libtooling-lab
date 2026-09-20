"""Create genuine multiple-includer compile contexts through normal imports."""
import json


def second_includer(project, conflicting=False):
    root, source = project.roots["alpha"], project.sources["alpha"]
    second = source.parent / "other.cpp"
    second.write_text('#include "common.hpp"\nint other() { return alpha::helper(); }\n')
    commands_path = root / "compile_commands.json"
    commands = json.loads(commands_path.read_text())
    arguments = [str(second) if value == str(source) else value
                 for value in commands[0]["arguments"]]
    if conflicting:
        arguments.append("-DHEADER_VARIANT=2")
    commands.append({"directory": str(root), "file": str(second), "arguments": arguments})
    commands_path.write_text(json.dumps(commands))
    clone = project.rows("SELECT active_clone_id FROM repository WHERE name='alpha'")[0][0]
    project.cli("import", "-p", root, "--existing-clone", clone)
    project.cli("extract", "-o", project.facts["alpha"], source, second)
    return source.parent / "common.hpp"
