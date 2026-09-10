import json
from pytest_bdd import given, when, parsers


def _run(defaults, family, mode, root):
    source = root / "src/main.cpp"
    db = root / (family + ".conf.db")
    cli_include = defaults.root / "cli-headers"
    cli_forced = defaults.root / "cli forced.hpp"
    extras = {
        "explicit": [
            f"--extra-arg=-DSELECTED=2 -std=c++23 -I {cli_include}",
            f"--extra-arg=-include '{cli_forced}'",
        ],
        "empty": ["--extra-arg="],
        "whitespace": ["--extra-arg=   "],
    }.get(mode, [])
    selectors = defaults.args
    if family == "import":
        return defaults.run(
            "import",
            "--conf",
            db,
            "--component",
            "app=src",
            "-p",
            root,
            *selectors,
            *extras,
        )
    if mode != "empty":
        imported = defaults.run(
            "import",
            "--conf",
            db,
            "--component",
            "app=src",
            "-p",
            root,
            *selectors,
            *extras,
        )
        assert imported.returncode == 0, imported.stderr
    output = root / "out" / (family + ".db")
    output.parent.mkdir(exist_ok=True)
    command = ["extract"] if family == "extract" else ["analyse", "dependency"]
    return defaults.run(
        *command, "--conf", db, "-o", output, *selectors, *extras, source
    )


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
    (defaults.root / "yaml forced.hpp").write_text("#define FORCED 1\n")
    (defaults.root / "cli forced.hpp").write_text("#define FORCED 2\n")
    system_include = defaults.root / "system-headers"
    system_include.mkdir()
    (system_include / "retained.hpp").write_text("struct RetainedSystemHeader {};\n")
    source = source_root / "main.cpp"
    source.write_text(
        '#if SELECTED == 1\n#include "yaml.hpp"\n#endif\n'
        '#if SELECTED == 2\n#include "cli.hpp"\n'
        "static_assert(__cplusplus > 202002L);\n#endif\n"
        "#ifndef RETAINED\n#error unrelated YAML macro lost\n#endif\n"
        "static_assert(FORCED == SELECTED);\n"
        "#include <retained.hpp>\n"
        "struct RetainedYamlDefault {};\n"
    )
    (root / "compile_commands.json").write_text(
        json.dumps(
            [
                {
                    "directory": str(root),
                    "file": str(source),
                    "arguments": [
                        "clang++",
                        "-std=c++23",
                        "-Werror=macro-redefined",
                        str(source),
                    ],
                }
            ]
        )
    )
    target = "config-file" if tier == "env" else tier
    yaml_args = [
        "-DSELECTED=1",
        "-DRETAINED=1",
        "-std=c++17",
        "-I",
        str(yaml_include),
        "-include",
        str(defaults.root / "yaml forced.hpp"),
        "-isystem",
        str(system_include),
    ]
    defaults.write(
        target, extra_args=yaml_args, facts_template="{project_root}/out/{filename}.db"
    )
    if tier == "config-file":
        defaults.args += ["--config", str(defaults.files[tier])]
    elif tier == "env":
        defaults.env["FACTS_TOOL_CONFIG"] = str(defaults.files["config-file"])
    defaults.precedence_yaml = {
        path: (path.read_bytes(), path.stat().st_mtime_ns)
        for path in defaults.files.values()
        if path.exists()
    }
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
