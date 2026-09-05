from pytest_bdd import given, when, then, parsers
from support.facts_template import FactsTemplateProject

TEMPLATES = {"user": "{project_root}/.index/user-{filename}.db",
             "project": "{project_root}/.index/project-{filename}.db",
             "config-file": "{project_root}/.index/file-{filename}.db"}

@given(parsers.parse('facts_template declared at tiers "{tiers}"'))
def facts_tiers(defaults, tiers):
    names = tiers.split(",")
    for tier in names:
        defaults.write(tier, facts_template=TEMPLATES[tier])
        if tier == "config-file":
            defaults.args += ["--config", str(defaults.files[tier])]
    defaults.template_project = FactsTemplateProject(defaults, None)

@when(parsers.parse('I extract one source with "{explicit}" output'))
def extract_with(defaults, explicit):
    project = defaults.template_project
    output = defaults.root / "explicit.db" if explicit == "explicit" else None
    defaults.last = project.extract(project.sources[0], output=output)
    defaults.explicit_output = output

@then(parsers.parse('only the "{winner}" facts database exists'))
def only_winner(defaults, winner):
    assert defaults.last.returncode == 0, defaults.last.stderr
    written = {tier for tier in TEMPLATES
               if (defaults.cwd / ".index" / (tier.replace("config-file", "file") + "-a.db")).is_file()}
    if winner == "explicit":
        assert defaults.explicit_output.is_file() and not written, written
    else:
        assert written == {winner}, written

@given("an absolute literal conf_template below the project root")
def absolute_literal(defaults):
    defaults.expected_path = defaults.cwd / ".index/project.db"
    defaults.write(conf_template=str(defaults.expected_path))

@given("a facts_template that escapes HOME through a symlink after ~/")
def tilde_escape(defaults):
    outside = defaults.root / "escaped-outside"
    outside.mkdir()
    home = defaults.root / "home"
    home.mkdir(exist_ok=True)
    (home / "linked").symlink_to(outside)
    defaults.template_project = FactsTemplateProject(defaults, "~/linked/{filename}.db")
    defaults.before = defaults.snapshot()

@then("no facts database was created anywhere")
def nothing_created(defaults):
    assert defaults.last.returncode == 3, defaults.last.stderr
    assert defaults.snapshot() == defaults.before

@given(parsers.parse('a user conf_template "{lower}" overridden by project conf_template "{upper}"'))
def overridden(defaults, lower, upper):
    defaults.write("user", conf_template=lower)
    defaults.write("project", conf_template=upper)

@given("a project facts_template but no imported configuration database")
def unimported(defaults):
    defaults.template_project = FactsTemplateProject(
        defaults, "{project_root}/.index/{relative_path}/{filename}.db", import_project=False)
    defaults.before = defaults.snapshot()

@given("a project facts_template whose configuration database is corrupt")
def corrupt(defaults):
    project = FactsTemplateProject(defaults, "{project_root}/.index/{relative_path}/{filename}.db")
    defaults.template_project = project
    conf = defaults.root / "home/.local/share/facts-tool" / defaults.cwd.parent.relative_to("/") / (defaults.cwd.name + ".db")
    conf.write_bytes(b"not a sqlite database")
    defaults.before = defaults.snapshot()

@then("extraction fails with a runtime error and the filesystem is unchanged")
def runtime_unchanged(defaults):
    assert defaults.last.returncode == 1, defaults.last.stderr
    assert not (defaults.cwd / ".index").exists()
    assert defaults.snapshot() == defaults.before
