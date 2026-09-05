import json
from pytest_bdd import given, when, then


@given("a direct-conf fixture with a project facts_template")
def direct_conf_fixture(defaults):
    root = defaults.cwd
    source = root / "main.cpp"
    source.write_text("struct DirectConf {};\n")
    (root / "compile_commands.json").write_text(json.dumps([{
        "directory": str(root), "file": str(source),
        "arguments": ["clang++", "-std=c++23", str(source)]}]))
    defaults.write(facts_template="{project_root}/facts.db")
    conf = defaults.root / "explicit-conf.db"
    imported = defaults.run("import", "--conf", conf, "--component", "app=.",
                            "-p", root)
    assert imported.returncode == 0, imported.stderr
    defaults.direct_conf = (conf, source)


@when("I extract and list symbols with only --conf")
def direct_conf_symbols(defaults):
    conf, source = defaults.direct_conf
    extracted = defaults.run("extract", "--conf", conf, source)
    assert extracted.returncode == 0, extracted.stderr
    defaults.last = defaults.run("symbol", "list", "--conf", conf)


@then("symbol listing succeeds from the YAML facts_template")
def assert_direct_conf_symbols(defaults):
    assert defaults.last.returncode == 0, defaults.last.stderr
    assert "DirectConf" in defaults.last.stdout
