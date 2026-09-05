from pytest_bdd import given, when, then, parsers
from support.facts_template import FactsTemplateProject

@given(parsers.parse('HOME is a directory named "{name}" and conf_template "{template}"'))
def braced_home(defaults, name, template):
    home = defaults.root / name
    home.mkdir()
    defaults.env["HOME"] = str(home)
    defaults.write(conf_template=template)
    defaults.expected_path = home / template[2:]

@given(parsers.parse('HOME is a directory named "{name}" and a facts_template "{template}"'))
def braced_home_facts(defaults, name, template):
    home = defaults.root / name
    home.mkdir()
    defaults.env["HOME"] = str(home)
    defaults.template_project = FactsTemplateProject(defaults, template)
    defaults.expected_path = home / "a.db"

@then("the facts database was written at the expected path")
def facts_written(defaults):
    assert defaults.last.returncode == 0, defaults.last.stderr
    assert defaults.expected_path.is_file(), defaults.last.stderr

@given(parsers.parse('an environment variable naming a whole "{key}" path'))
def whole_path_env(defaults, key):
    defaults.expected_path = defaults.cwd / ".index" / ("whole-" + key + ".db")
    defaults.env["FACTS_TOOL_WHOLE"] = str(defaults.expected_path)
    if key == "conf_template":
        defaults.write(conf_template="${FACTS_TOOL_WHOLE}")
    else:
        defaults.template_project = FactsTemplateProject(defaults, "${FACTS_TOOL_WHOLE}")

@given(parsers.parse('a user "{key}" with a substituted .. overridden by a valid project "{key2}"'))
def substituted_dotdot(defaults, key, key2):
    defaults.env["FACTS_TOOL_PART"] = ".."
    lower = "${FACTS_TOOL_PART}/outside.db" if key == "conf_template" else \
        "{project_root}/${FACTS_TOOL_PART}/{filename}.db"
    defaults.write("user", **{key: lower})
    defaults.write("project", **{key2: "{project_root}/.index/{filename}.db"})
