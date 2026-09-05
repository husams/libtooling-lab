import json
import sqlite3

class CompileDefaults:
    def __init__(self, defaults, override, input_kind):
        self.d, self.kind = defaults, input_kind
        self.source = defaults.cwd / "unit.cpp"
        self.header = defaults.cwd / "yaml header.hpp"
        self.header.write_text("#define YAML_SEEN 1\n")
        self.source.write_text("struct Initial {};\n")
        # A user-tier extra_args entry proves the merge order (user, then
        # project, then CLI): -DVALUE=0 here is overridden by the project's
        # -DVALUE=2 and then the CLI's -DVALUE=3, so require_effect(3)
        # below only passes if that order actually reaches the compiler.
        defaults.write("user", extra_args=["-DVALUE=0"])
        defaults.write(conf_root=str(defaults.root / "store"),
                       conf_template="{filename}.db",
                       extra_args=["-DVALUE=2", "-include", str(self.header),
                                   "-DSPACE=value with spaces"])
        self.db = defaults.root / "direct.db" if override != "generated" else (
            defaults.root / "store" / (defaults.cwd.name + ".db"))
        if override == "cli": defaults.args = ["--conf", str(self.db)]
        if override == "env": defaults.env["FACTS_TOOL_CONF"] = str(self.db)
        self.base = ["clang++", "-std=c++23", "-DVALUE=1", str(self.source)]
        (defaults.cwd / "compile_commands.json").write_text(json.dumps([{
            "directory": str(defaults.cwd), "file": str(self.source),
            "arguments": self.base}]))
        self.cli = ["--extra-arg=-DVALUE=3 '-DCLI_SPACE=two words'"]
        self.result = self.run("import")
        assert self.result.returncode == 0, self.result.stderr
        self.original = self.options()

    def options(self):
        with sqlite3.connect(self.db) as db:
            return db.execute("SELECT driver,compile_options FROM file WHERE driver IS NOT NULL").fetchall()

    def run(self, family, cli=True):
        d = self.d
        if family == "import":
            cmd = ["import", "--component", "fixture=."]
            if self.kind == "json": cmd += ["-p", str(d.cwd)]
            else: cmd += [str(self.source)]
        else:
            cmd = ["extract"] if family == "extract" else ["analyse", "dependency"]
            cmd += ["-o", str(d.root / (family + ".db")), str(self.source)]
        return d.run(*cmd, *d.args, *(self.cli if cli else []))

    def require_effect(self, value):
        self.source.write_text(f"#if VALUE != {value} || !YAML_SEEN\n"
            '#include "wrong_argument_order.h"\n#endif\nstruct Ordered {};\n')

    def verify(self):
        shown = self.d.show()
        assert shown.returncode == 0, shown.stderr
        line = next(x for x in shown.stdout.splitlines() if x.startswith("extra_args: "))
        assert line.index("[-DVALUE=0]") < line.index("[-include]"), line
        assert self.options() == self.original
        result = self.d.run("component", "compile-commands", "fixture",
                            "--conf", str(self.db))
        assert result.returncode == 0, result.stderr
        args = json.loads(result.stdout)[0]["arguments"]
        assert args.count("-DVALUE=3") == 1 and "-DVALUE=2" not in args, args
        assert "-include" not in args and "-DSPACE=value with spaces" not in args
        assert args[-2:] == ["-DVALUE=3", "-DCLI_SPACE=two words"], args
        if self.kind == "json":
            normalized = [args[0], "--driver-mode=g++", *self.base[1:]]
            assert args == normalized + args[-2:], args
        with sqlite3.connect(self.db) as db:
            names = {x[0] for x in db.execute("SELECT name FROM file")}
        assert self.header.name in names
