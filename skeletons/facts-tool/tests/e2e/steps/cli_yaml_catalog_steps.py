import json
from pytest_bdd import given, when, then, parsers


@given("a catalog path precedence fixture")
def catalog_fixture(defaults):
    root = defaults.cwd
    source = root / "main.cpp"
    source.write_text("struct CatalogPath {};\n")
    (root / "compile_commands.json").write_text(json.dumps([{
        "directory": str(root), "file": str(source),
        "arguments": ["clang++", "-std=c++23", str(source)]}]))
    defaults.write(conf_root=str(root / "yaml-store"),
                   conf_template="yaml.db")
    conf = defaults.root / "catalog-explicit.db"
    imported = defaults.run("import", "--conf", conf, "--component", "app=."
                            , "-p", root)
    assert imported.returncode == 0, imported.stderr
    defaults.catalog_conf = conf
    defaults.yaml_conf = root / "yaml-store/yaml.db"


@when(parsers.parse('I run "{command}" with "{placement}" and "{alias}" configuration'))
def run_catalog(defaults, command, placement, alias):
    parts = command.split()
    group, leaf = parts[0], parts[1:]
    if placement == "group":
        args = [group, alias, defaults.catalog_conf, *leaf]
    else:
        args = [group, *leaf, alias, defaults.catalog_conf]
    defaults.catalog_command = command
    defaults.last = defaults.run(*args)


@then("the configured catalog command succeeds")
def assert_catalog(defaults):
    assert defaults.last.returncode == 0, defaults.last.stderr
    assert not defaults.yaml_conf.exists()
    if defaults.catalog_command == "config show":
        assert f'conf: "{defaults.catalog_conf}"' in defaults.last.stdout
