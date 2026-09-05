import sqlite3
from pytest_bdd import then, parsers


@then(parsers.parse('only the "{expected}" compiler token is observed for "{family}"'))
def assert_precedence(defaults, expected, family):
    root, _ = defaults.precedence
    _, _, _, result = defaults.precedence_result
    if expected == "error":
        assert result.returncode != 0
        return
    database = root / (family + ".conf.db")
    if family == "import":
        with sqlite3.connect(database) as connection:
            options = [row[0] for row in connection.execute(
                "SELECT compile_options FROM file WHERE name='main.cpp'")]
            names = {row[0] for row in connection.execute("SELECT name FROM file")}
        assert bool(options) and ("-DCLI_TOKEN=1" in options[0]) == (expected == "cli")
        expected_name = "cli.hpp" if expected == "cli" else "yaml.hpp"
        if expected == "none":
            assert not {"yaml.hpp", "cli.hpp"} & names
        else:
            omitted = "yaml.hpp" if expected == "cli" else "cli.hpp"
            assert expected_name in names and omitted not in names
        return
    if family == "extract":
        with sqlite3.connect(root / "out/extract.db") as connection:
            names = {row[0] for row in connection.execute(
                "SELECT qualified_name FROM symbol")}
    else:
        with sqlite3.connect(root / "out/dependency.db") as output, sqlite3.connect(
                root / "dependency.conf.db") as project:
            files = dict(project.execute("SELECT id,name FROM file"))
            names = {files[row[1]] for row in output.execute(
                "SELECT src_file_id,dst_file_id FROM include_dependency")}
    expected_name = ("YamlHeader" if expected == "yaml" else "CliHeader") \
        if family == "extract" else ("yaml.hpp" if expected == "yaml" else "cli.hpp")
    omitted = ("CliHeader" if expected == "yaml" else "YamlHeader") \
        if family == "extract" else ("cli.hpp" if expected == "yaml" else "yaml.hpp")
    if expected == "none":
        assert not {"YamlHeader", "CliHeader", "yaml.hpp", "cli.hpp"} & names
    else:
        assert expected_name in names and omitted not in names
