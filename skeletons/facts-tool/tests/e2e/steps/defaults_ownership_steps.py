import concurrent.futures
import sqlite3
from pytest_bdd import given, when, then, parsers

@given("two marked projects map to one generated database")
def projects(defaults):
    defaults.other = defaults.root / "other"
    defaults.other.mkdir()
    (defaults.other / ".git").mkdir()
    defaults.write(conf_root=str(defaults.root / "shared"), conf_template="same.db")
    (defaults.other / ".facts-tool.yaml").write_bytes(defaults.files["project"].read_bytes())
    defaults.owned_db = defaults.root / "shared/same.db"

def register(defaults, cwd, name):
    path = cwd / name
    path.mkdir(exist_ok=True)
    return defaults.run("component", "add", "--name", name, "--path", path,
                        "--kind", "external", cwd=cwd)

@when(parsers.parse('I initialize generated ownership with "{mode}"'))
def initialize(defaults, mode):
    if mode == "existing":
        defaults.owned_db.parent.mkdir()
        with sqlite3.connect(defaults.owned_db) as db:
            db.execute("CREATE TABLE unrelated(value TEXT)")
            db.execute("INSERT INTO unrelated VALUES('preserve')")
        defaults.original = defaults.owned_db.read_bytes()
        defaults.results = [register(defaults, defaults.cwd, "first")]
    elif mode == "repeat":
        defaults.results = [register(defaults, defaults.cwd, "first"),
                            register(defaults, defaults.cwd, "second")]
    elif mode == "collision":
        defaults.results = [register(defaults, defaults.cwd, "first"),
                            register(defaults, defaults.other, "second")]
    else:
        with concurrent.futures.ThreadPoolExecutor(2) as executor:
            second = defaults.other if mode == "concurrent-collision" else defaults.cwd
            futures = [executor.submit(register, defaults, cwd, name)
                       for cwd, name in [(defaults.cwd, "first"), (second, "second")]]
            defaults.results = [f.result() for f in futures]
    defaults.mode = mode

@then("database ownership is serialized and never adopted")
def ownership(defaults):
    results = defaults.results
    codes = sorted(r.returncode for r in results)
    expected = {"existing": [3], "repeat": [0, 0], "collision": [0, 3],
                "concurrent": [0, 0], "concurrent-collision": [0, 3]}
    assert codes == expected[defaults.mode], [(r.returncode, r.stderr) for r in results]
    for r in results:
        if r.returncode:
            assert "generated conf path collision:" in r.stderr, r.stderr
    if defaults.mode == "existing":
        assert defaults.owned_db.read_bytes() == defaults.original
    else:
        with sqlite3.connect(defaults.owned_db) as db:
            owners = db.execute("SELECT project_root FROM generated_conf_owner").fetchall()
        assert len(owners) == 1 and owners[0][0] in map(str, [defaults.cwd, defaults.other])
    assert {p.name for p in defaults.owned_db.parent.iterdir()} == {"same.db"}
