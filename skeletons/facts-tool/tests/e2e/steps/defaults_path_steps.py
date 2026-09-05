import json
from pytest_bdd import given, when, then, parsers
from support.facts_template import FactsTemplateProject

@given(parsers.parse('a project identity fixture "{kind}"'))
def identity(defaults, kind):
    defaults.kind = kind
    root = defaults.cwd
    if kind == "unmarked": (root / ".git").rmdir()
    if kind == "git-file":
        (root / ".git").rmdir()
        (root / ".git").write_text("gitdir: unused")
    if kind == "yaml-marker":
        (root / ".git").rmdir()
        defaults.write()
    defaults.identity = root
    if kind in ("nested", "git-file", "yaml-marker"):
        defaults.cwd = root / "sub/nested"
        defaults.cwd.mkdir(parents=True)
    if kind == "symlink":
        link = defaults.root / "linked"
        link.symlink_to(root, target_is_directory=True)
        defaults.cwd = link
    if kind == "filesystem-root":
        defaults.cwd = defaults.identity = root.anchor and type(root)("/")
    if kind == "unicode":
        renamed = root.parent / "acmé 空間.v2"
        root.rename(renamed)
        defaults.cwd = defaults.identity = renamed
    defaults.expected_path = (defaults.root / "home/.local/share/facts-tool" /
        defaults.identity.parent.relative_to("/") /
        ((defaults.identity.name or "_root") + ".db"))

@then("the canonical project identity determines the database")
def identity_result(defaults):
    assert defaults.value("project_root") == str(defaults.identity.resolve())
    assert defaults.value("conf") == str(defaults.expected_path)
    assert defaults.snapshot() == defaults.before

@given(parsers.parse('template "{template}" with expected relative target "{target}"'))
def template(defaults, template, target):
    defaults.write(conf_root="store", conf_template=template)
    defaults.expected_path = defaults.cwd / "store" / target

@then("the rendered path is canonical and no storage was created")
def rendered(defaults):
    assert defaults.value("conf") == str(defaults.expected_path), defaults.last.stdout
    assert not (defaults.cwd / "store").exists()

@given(parsers.parse('invalid template "{template}"'))
def invalid(defaults, template):
    defaults.write(conf_root="store", conf_template=template)

@given("a generated target symlink escapes storage")
def escape(defaults):
    (defaults.cwd / "store").mkdir()
    (defaults.cwd / "outside").mkdir()
    (defaults.cwd / "store/link").symlink_to(defaults.cwd / "outside")
    defaults.write(conf_root="store", conf_template="link/escaped.db")

@given(parsers.parse('configuration environment case "{case}"'))
def environment(defaults, case):
    defaults.env.pop("HOME", None)
    if case == "xdg-data": defaults.env["XDG_DATA_HOME"] = str(defaults.root / "data")
    if case == "explicit-root": defaults.write(conf_root=str(defaults.root / "data"))
    if case == "missing-key": defaults.write()
    if case == "tilde": defaults.write(conf_root="~/data")
    if case == "relative-config": defaults.env["XDG_CONFIG_HOME"] = "relative"
    if case == "relative-data": defaults.env["XDG_DATA_HOME"] = "relative"
    if case == "relative-data-missing-key":
        defaults.env["XDG_DATA_HOME"] = "relative"
        defaults.write()

@then("missing HOME remains symbolic in discovery")
def symbolic(defaults):
    assert "${HOME}/.config/facts-tool/config.yaml" in defaults.last.stdout + defaults.last.stderr
    assert "HOME is unset" in defaults.last.stdout + defaults.last.stderr
    assert defaults.snapshot() == defaults.before

@given("a user file with a relative conf_root")
def user_relative_root(defaults):
    defaults.write("user", conf_root=".index", conf_template="{filename}.db")

@then("conf is under the project root's generated storage")
def anchored_to_project_root(defaults):
    assert defaults.value("conf") == str(defaults.cwd / ".index" / (defaults.cwd.name + ".db"))
    assert not (defaults.root / "home/.config/facts-tool/.index").exists()

@given(parsers.parse('conf_template "{template}" with an environment token'))
def template_with_token(defaults, template):
    defaults.env["FACTS_TOOL_TEST_TOKEN"] = "envtoken"
    defaults.write(conf_root="store", conf_template=template)
    filled = template.replace("{project_name}", defaults.cwd.name) \
        .replace("{user}", defaults.env.get("USER") or defaults.env.get("LOGNAME") or "") \
        .replace("${FACTS_TOOL_TEST_TOKEN}", "envtoken") \
        .replace("{filename}", defaults.cwd.name)
    defaults.expected_path = defaults.cwd / "store" / filled

@then("conf matches the expected rendered path")
def token_rendered(defaults):
    assert defaults.value("conf") == str(defaults.expected_path), defaults.last.stdout

@given(parsers.parse('conf_template "{template}"'))
def bare_template(defaults, template):
    defaults.write(conf_root="store", conf_template=template)

@given("a project facts_template using the source placeholder")
def facts_template_project(defaults):
    defaults.template_project = FactsTemplateProject(
        defaults, "{project_root}/.index/{relative_path}/{filename}.db")

@when("I extract with no explicit output and one source")
def extract_one(defaults):
    defaults.last = defaults.template_project.extract(defaults.template_project.sources[0])

@when("I extract with no explicit output and two sources")
def extract_two(defaults):
    defaults.last = defaults.template_project.extract(*defaults.template_project.sources)

@when("I extract with an explicit output and one source")
def extract_explicit(defaults):
    defaults.explicit_output = defaults.root / "explicit.db"
    defaults.last = defaults.template_project.extract(
        defaults.template_project.sources[0], output=defaults.explicit_output)

@then("extraction succeeds and writes the facts_template path")
def wrote_facts_template(defaults):
    assert defaults.last.returncode == 0, defaults.last.stderr
    assert (defaults.cwd / ".index/src/a.db").is_file()

@then("extraction fails with a usage error asking for -o/--facts")
def usage_error(defaults):
    assert defaults.last.returncode == 2, defaults.last.stderr
    assert "-o" in defaults.last.stderr and "--facts" in defaults.last.stderr

@then("extraction succeeds and writes the explicit output")
def wrote_explicit(defaults):
    assert defaults.last.returncode == 0, defaults.last.stderr
    assert defaults.explicit_output.is_file()

@given("the acceptance-example conf_template and facts_template")
def acceptance_example(defaults):
    defaults.write(conf_template="{project_root}/.index/project.db",
                   facts_template="{project_root}/.index/{relative_path}/{filename}.db")

@then("conf is the project root's .index/project.db")
def conf_is_index_project(defaults):
    assert defaults.value("conf") == str(defaults.cwd / ".index/project.db"), defaults.last.stdout

@then("extracting one source writes the project root's .index/src/a/b.db")
def extract_writes_index_src(defaults):
    project = FactsTemplateProject(defaults, "{project_root}/.index/{relative_path}/{filename}.db")
    result = project.extract(project.sources[0])
    assert result.returncode == 0, result.stderr
    assert (defaults.cwd / ".index/src/a.db").is_file()

@then("conf is directly under HOME")
def conf_under_home(defaults):
    assert defaults.value("conf") == str(defaults.root / "home/project.db"), defaults.last.stdout

@given("a project facts_template that escapes through a symlink after {project_root}")
def escaping_facts_template(defaults):
    outside = defaults.root / "escaped-outside"
    outside.mkdir()
    (defaults.cwd / "linked").symlink_to(outside)
    defaults.template_project = FactsTemplateProject(
        defaults, "{project_root}/linked/{relative_path}/{filename}.db")

@then(parsers.parse('extraction fails with "{message}"'))
def extraction_fails_with(defaults, message):
    assert defaults.last.returncode == 3, defaults.last.stderr
    assert message in defaults.last.stderr, defaults.last.stderr

@given("a project facts_template using the source placeholder registered under a direct conf")
def facts_template_direct(defaults):
    defaults.template_project = FactsTemplateProject(
        defaults, "{project_root}/.index/{relative_path}/{filename}.db",
        conf=defaults.root / "direct.db")

@when("I extract under the direct conf with no explicit output and one source")
def extract_under_direct(defaults):
    defaults.last = defaults.template_project.extract(defaults.template_project.sources[0])

@when("I extract with no explicit output and a source that was never imported")
def extract_unimported(defaults):
    missing = defaults.cwd / "src/missing.cpp"
    missing.write_text("struct S {};\n")
    defaults.last = defaults.template_project.extract(missing)

@then("extraction fails and no facts_template directory was created")
def extraction_failed_no_directory(defaults):
    assert defaults.last.returncode != 0, defaults.last.stdout
    assert not (defaults.cwd / ".index").exists()

@given("a user conf_template with an unknown placeholder and a valid project conf_template")
def invalid_lower_tier(defaults):
    defaults.write("user", conf_template="{unknown}.db")
    defaults.write("project", conf_template="{filename}.db")

@given("a malformed project file and a valid user file")
def malformed_project_valid_user(defaults):
    defaults.files["project"].write_text("conf_root: [bad")
    defaults.write("user", conf_root="store")

@then("discovery marks the project file invalid and the user file found")
def discovery_marks_both(defaults):
    assert str(defaults.files["project"]) + " [invalid]" in defaults.last.stdout
    assert str(defaults.files["user"]) + " [found]" in defaults.last.stdout

@when("I analyse dependencies with no explicit output and one source")
def dependency_default(defaults):
    defaults.last = defaults.template_project.dependency(defaults.template_project.sources[0])

@then("dependency analysis succeeds and writes the facts_template path")
def dependency_wrote_facts_template(defaults):
    assert defaults.last.returncode == 0, defaults.last.stderr
    assert (defaults.cwd / ".index/src/a.db").is_file()

@when("I list symbols with no explicit --facts")
def symbol_default(defaults):
    defaults.last = defaults.template_project.symbol_list()

@then("symbol listing fails with a usage error asking for -o/--facts")
def symbol_usage_error(defaults):
    assert defaults.last.returncode == 2, defaults.last.stderr
    assert "-o" in defaults.last.stderr and "--facts" in defaults.last.stderr

@given("a project-scoped facts_template with no source placeholder")
def project_scoped_facts_template(defaults):
    defaults.template_project = FactsTemplateProject(
        defaults, "{project_root}/.index/project-facts.db")
    extracted = defaults.template_project.extract(defaults.template_project.sources[0])
    assert extracted.returncode == 0, extracted.stderr

@then("symbol listing succeeds using the facts_template path")
def symbol_used_facts_template(defaults):
    assert defaults.last.returncode == 0, defaults.last.stderr
    assert (defaults.cwd / ".index/project-facts.db").is_file()
