import json
from pytest_bdd import given, when, then, parsers

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
