import json
from pytest_bdd import given, when, then, parsers


def _project(defaults):
    root = defaults.cwd
    source = root / "main.cpp"
    source.write_text("struct YamlSymbol {};\n")
    (root / "compile_commands.json").write_text(json.dumps([{
        "directory": str(root), "file": str(source),
        "arguments": ["clang++", "-std=c++23", str(source)]}]))
    defaults.write(facts_template="{project_root}/yaml-facts.db")
    imported = defaults.run("import", "--component", "app=.", "-p", root)
    assert imported.returncode == 0, imported.stderr
    return root, source


@given("an output path precedence fixture")
def output_fixture(defaults):
    defaults.path_project = _project(defaults)


@when(parsers.parse('I run "{family}" with "{alias}" output'))
def run_output(defaults, family, alias):
    root, source = defaults.path_project
    explicit = root / "explicit-output.db"
    command = ["extract"] if family == "extract" else ["analyse", "dependency"]
    defaults.last = defaults.run(*command, alias, explicit, source)
    defaults.explicit_path, defaults.yaml_path = explicit, root / "yaml-facts.db"


@when(parsers.parse('I run "{family}" with an empty "{alias}" output'))
def run_empty_output(defaults, family, alias):
    root, source = defaults.path_project
    command = ["extract"] if family == "extract" else ["analyse", "dependency"]
    defaults.last = defaults.run(*command, alias, "", source)
    defaults.yaml_path = root / "yaml-facts.db"


@then("the explicit output path wins")
def assert_output(defaults):
    assert defaults.last.returncode == 0, defaults.last.stderr
    assert defaults.explicit_path.is_file()
    assert not defaults.yaml_path.exists()


@then("the explicit empty output is rejected")
def assert_empty_output(defaults):
    assert defaults.last.returncode != 0
    assert not defaults.yaml_path.exists()


@when("I run extract with --output at the fallback path")
def run_equal_output(defaults):
    root, source = defaults.path_project
    path = root / "yaml-facts.db"
    defaults.last = defaults.run("extract", "--output", path, source)
    defaults.equal_path = path


@then("the equal fallback output succeeds")
def assert_equal_output(defaults):
    assert defaults.last.returncode == 0, defaults.last.stderr
    assert defaults.equal_path.is_file()


@when(parsers.parse('I run "{family}" with repeated "{first}" and "{second}" output'))
def run_repeated_output(defaults, family, first, second):
    root, source = defaults.path_project
    command = ["extract"] if family == "extract" else ["analyse", "dependency"]
    defaults.before_duplicate = defaults.snapshot()
    defaults.duplicate_paths = (root / "first.db", root / "second.db")
    defaults.last = defaults.run(*command, first, defaults.duplicate_paths[0],
                                 second, defaults.duplicate_paths[1], source)


@then("the repeated output is rejected without mutation")
def assert_repeated_output(defaults):
    assert defaults.last.returncode == 2, defaults.last.stderr
    assert not any(path.exists() for path in defaults.duplicate_paths)
    assert not (defaults.path_project[0] / "yaml-facts.db").exists()
    assert defaults.snapshot() == defaults.before_duplicate
