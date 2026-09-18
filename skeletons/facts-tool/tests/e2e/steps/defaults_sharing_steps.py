import concurrent.futures
import sqlite3
from pytest_bdd import given, when, then, parsers

@given("two marked projects map to one generated database")
def projects(defaults):
    defaults.other = defaults.root / "other"
    defaults.other.mkdir()
    (defaults.other / ".git").mkdir()
    defaults.write(conf_root=str(defaults.root / "shared"),
                   conf_template="same.db",
                   facts_template=str(defaults.root / "shared-facts.db"))
    (defaults.other / ".facts-tool.yaml").write_bytes(defaults.files["project"].read_bytes())
    defaults.shared_db = defaults.root / "shared/same.db"

def register(defaults, cwd, name):
    path = cwd / name
    path.mkdir(exist_ok=True)
    return defaults.run("component", "add", "--name", name, "--path", path,
                        "--kind", "external", cwd=cwd)

@when(parsers.parse('I initialize the shared database with "{mode}"'))
def initialize(defaults, mode):
    if mode == "existing":
        defaults.shared_db.parent.mkdir()
        with sqlite3.connect(defaults.shared_db) as db:
            db.execute("CREATE TABLE unrelated(value TEXT)")
            db.execute("INSERT INTO unrelated VALUES('preserve')")
        defaults.original = defaults.shared_db.read_bytes()
        defaults.results = [register(defaults, defaults.cwd, "first")]
    elif mode == "repeat":
        defaults.results = [register(defaults, defaults.cwd, "first"),
                            register(defaults, defaults.cwd, "second")]
    elif mode == "shared-root":
        defaults.results = [register(defaults, defaults.cwd, "first"),
                            register(defaults, defaults.other, "second")]
    else:
        with concurrent.futures.ThreadPoolExecutor(2) as executor:
            second = defaults.other if mode == "concurrent-shared-root" else defaults.cwd
            futures = [executor.submit(register, defaults, cwd, name)
                       for cwd, name in [(defaults.cwd, "first"), (second, "second")]]
            defaults.results = [f.result() for f in futures]
    defaults.mode = mode

@then("database sharing is serialized and unrelated databases are never adopted")
def shared_database(defaults):
    results = defaults.results
    codes = sorted(r.returncode for r in results)
    expected = {"existing": [3], "repeat": [0, 0], "shared-root": [0, 0],
                "concurrent": [0, 0], "concurrent-shared-root": [0, 0]}
    assert codes == expected[defaults.mode], [(r.returncode, r.stderr) for r in results]
    for r in results:
        if r.returncode:
            assert "not a project configuration database:" in r.stderr, r.stderr
    if defaults.mode == "existing":
        assert defaults.shared_db.read_bytes() == defaults.original
    else:
        with sqlite3.connect(defaults.shared_db) as db:
            assert db.execute(
                "SELECT name FROM sqlite_master WHERE name='generated_conf_owner'"
            ).fetchall() == []
            components = db.execute(
                "SELECT name FROM component WHERE kind='external' AND path != '/'"
            ).fetchall()
        assert set(components) == {("first",), ("second",)}
    assert {p.name for p in defaults.shared_db.parent.iterdir()} == {"same.db"}
