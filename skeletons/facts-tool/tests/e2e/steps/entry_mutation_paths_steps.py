from pytest_bdd import then

from support.database import require
from support.entries import run


@then("S-027 mutations reject colliding or empty facts paths without writes")
def collisions(context):
    alias = context.entry_root / "conf-alias.sqlite"
    alias.symlink_to(context.files_database_path)
    before = context.files_database_path.read_bytes()
    for command in (["import", "-p", context.entry_root],
                    ["file", "set-option", "--match", "app.cpp$", "--arg=-DS027_BAD=1"]):
        for path in (context.files_database_path, alias, ""):
            result = run(context, *command, "--conf", context.files_database_path,
                         "--facts", path, "-v", "0")
            require(result.returncode != 0, result.stdout + result.stderr)
            require(context.files_database_path.read_bytes() == before,
                    f"project changed after rejected facts path {path}")
