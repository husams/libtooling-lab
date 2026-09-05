import json
from pathlib import Path
from pytest_bdd import given, when, then, parsers


def _run(defaults, family, mode, root):
    source = root / "src/main.cpp"
    db = root / (family + ".conf.db")
    cli_include = defaults.root / "cli-headers"
    extras = {"explicit": [f"--extra-arg=-DCLI_TOKEN=1 -I {cli_include}"],
              "empty": ["--extra-arg="],
              "whitespace": ["--extra-arg=   "]}.get(mode, [])
    selectors = defaults.args
    if family == "import":
        return defaults.run("import", "--conf", db, "--component", "app=src",
                            "-p", root, *selectors, *extras)
    if mode != "empty":
        imported = defaults.run("import", "--conf", db, "--component", "app=src",
                                "-p", root, *selectors, *extras)
        assert imported.returncode == 0, imported.stderr
    output = root / "out" / (family + ".db")
    output.parent.mkdir(exist_ok=True)
    command = (["extract"] if family == "extract" else
               ["analyse", "dependency"])
    return defaults.run(*command, "--conf", db, "-o", output, *selectors,
                        *extras, source)


@given(parsers.parse('a CLI/YAML precedence fixture at "{tier}"'))
def precedence_fixture(defaults, tier):
    root = defaults.cwd
    source_root = root / "src"
    source_root.mkdir()
    yaml_include = defaults.root / "yaml-headers"
    yaml_include.mkdir()
    (yaml_include / "yaml.hpp").write_text("struct YamlHeader {};\n")
    cli_include = defaults.root / "cli-headers"
    cli_include.mkdir()
    (cli_include / "cli.hpp").write_text("struct CliHeader {};\n")
    source = source_root / "main.cpp"
    source.write_text('#ifdef YAML_TOKEN\n#include "yaml.hpp"\n#endif\n'
                      '#ifdef CLI_TOKEN\n#include "cli.hpp"\n#endif\n')
    (root / "compile_commands.json").write_text(json.dumps([{
        "directory": str(root), "file": str(source),
        "arguments": ["clang++", "-std=c++23", str(source)]}]))
    target = "config-file" if tier == "env" else tier
    defaults.write(target, extra_args=["-DYAML_TOKEN=1", "-I", str(yaml_include)],
                   facts_template="{project_root}/out/{filename}.db")
    if tier == "config-file":
        defaults.args += ["--config", str(defaults.files[tier])]
    elif tier == "env":
        defaults.env["FACTS_TOOL_CONFIG"] = str(defaults.files["config-file"])
        defaults.write("config-file", extra_args=["-DYAML_TOKEN=1", "-I",
                                                   str(yaml_include)],
                       facts_template="{project_root}/out/{filename}.db")
    defaults.precedence = (root, source)


@when(parsers.parse('I run "{family}" with "{mode}" CLI values'))
def run_precedence(defaults, family, mode):
    root, _ = defaults.precedence
    result = _run(defaults, family, mode, root)
    if mode == "empty":
        assert result.returncode != 0, result.stdout + result.stderr
        defaults.precedence_result = (family, mode, root, result)
        return
    assert result.returncode == 0, result.stdout + result.stderr
    defaults.precedence_result = (family, mode, root, result)

