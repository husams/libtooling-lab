import json

class FactsTemplateProject:
    """A tiny importable project used to exercise facts_template defaulting
    for extract/dependency/symbol: two registered sources under src/, so a
    test can pick either "exactly one source" (unambiguous) or "more than
    one" (usage error) when facts_template needs a per-source placeholder.
    An optional `conf` registers the project under a direct --conf path
    instead of the generated one, to prove facts_template still applies
    under a direct configuration-database override (B-030 C-3113).
    """

    def __init__(self, defaults, template, conf=None, tier="project",
                 sources=("src/a.cpp", "src/b.cpp"), import_project=True, **settings):
        self.d, self.conf = defaults, conf
        self.sources = []
        for name in sources:
            path = defaults.cwd / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text("struct S {};\n")
            self.sources.append(path)
        if template is not None:
            defaults.write(tier, facts_template=template, **settings)
        commands = [
            {"directory": str(defaults.cwd), "file": str(source),
             "arguments": ["clang++", "-std=c++23", str(source)]}
            for source in self.sources
        ]
        (defaults.cwd / "compile_commands.json").write_text(json.dumps(commands))
        if not import_project:
            return
        imported = defaults.run("import", "--component", "app=.", "-p", str(defaults.cwd),
                                *self._conf_args())
        assert imported.returncode == 0, imported.stderr

    def _conf_args(self):
        direct = ["--conf", str(self.conf)] if self.conf is not None else []
        return [*direct, *self.d.args]

    def extract(self, *sources, output=None):
        arguments = ["extract", *self._conf_args()]
        if output is not None:
            arguments += ["-o", str(output)]
        arguments += [str(source) for source in sources]
        return self.d.run(*arguments)

    def dependency(self, *sources, output=None):
        arguments = ["analyse", "dependency", *self._conf_args()]
        if output is not None:
            arguments += ["-o", str(output)]
        arguments += [str(source) for source in sources]
        return self.d.run(*arguments)

    def symbol_list(self):
        return self.d.run("symbol", "list", *self._conf_args())
