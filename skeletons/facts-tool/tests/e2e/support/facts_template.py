import json

class FactsTemplateProject:
    """A tiny importable project used to exercise facts_template defaulting
    for extract: two registered sources under src/, so a test can pick
    either "exactly one source" (unambiguous) or "more than one" (usage
    error) when facts_template needs a per-source placeholder.
    """

    def __init__(self, defaults, template):
        self.d = defaults
        self.sources = []
        for name in ("a.cpp", "b.cpp"):
            path = defaults.cwd / "src" / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text("struct S {};\n")
            self.sources.append(path)
        defaults.write(facts_template=template)
        commands = [
            {"directory": str(defaults.cwd), "file": str(source),
             "arguments": ["clang++", "-std=c++23", str(source)]}
            for source in self.sources
        ]
        (defaults.cwd / "compile_commands.json").write_text(json.dumps(commands))
        imported = defaults.run("import", "--component", "app=.", "-p", str(defaults.cwd))
        assert imported.returncode == 0, imported.stderr

    def extract(self, *sources, output=None):
        arguments = ["extract"]
        if output is not None:
            arguments += ["-o", str(output)]
        arguments += [str(source) for source in sources]
        return self.d.run(*arguments)
