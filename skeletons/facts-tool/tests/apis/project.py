"""A real C++ project; tests never inspect private database tables."""
import json


def create_project(root, compiler):
    root.mkdir(parents=True, exist_ok=True)
    source = root / "sample.cpp"
    header = root / "sample.hpp"
    header.write_text("#pragma once\ninline int answer() { return 42; }\n")
    source.write_text('#include "sample.hpp"\nint main() { return answer(); }\n')
    write_commands(root, compiler, [source])
    return source, header


def write_commands(root, compiler, sources):
    commands = [{"directory": str(root), "file": str(source),
                 "arguments": [compiler, "-std=c++17", "-c", str(source)]}
                for source in sources]
    (root / "compile_commands.json").write_text(json.dumps(commands))


def initialise(api, root):
    project = str(root / "project.db")
    facts = str(root / "facts.db")
    api.run(["-c", project, "-p", str(root)], "import")
    api.run(["-c", project, "-o", facts], "extract")
    return project, facts
