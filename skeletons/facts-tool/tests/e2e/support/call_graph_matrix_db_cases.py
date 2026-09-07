"""Database-error case builders for the outcome matrix (B-042)."""
import shutil
import sqlite3
import subprocess

from support.callgraph_run import command


def database_missing_facts(context, verbosity):
    argv = command(context, "--all", verbosity=verbosity)
    argv[argv.index("-f") + 1] = str(context.run_root_path / "matrix-missing.sqlite")
    return subprocess.run(argv, capture_output=True, text=True, env=context.recovery_env)


def database_empty_all(context, verbosity):
    empty = context.run_root_path / "matrix-empty.sqlite"
    shutil.copy2(context.facts_database_path, empty)
    with sqlite3.connect(empty) as connection:
        connection.execute("DELETE FROM relation_site WHERE kind IN (1,18)")
    argv = command(context, "--all", verbosity=verbosity)
    argv[argv.index("-f") + 1] = str(empty)
    return subprocess.run(argv, capture_output=True, text=True, env=context.recovery_env)


def break_project_index(context):
    """Renaming a column the recovery context reads makes recovery fail after
    the first traversal step, with the facts store still writable."""
    with sqlite3.connect(context.files_database_path) as connection:
        connection.execute("ALTER TABLE matched_symbol_index RENAME COLUMN usr TO usr_broken")


def restore_project_index(context):
    with sqlite3.connect(context.files_database_path) as connection:
        connection.execute("ALTER TABLE matched_symbol_index RENAME COLUMN usr_broken TO usr")


def failed_after_traversal(context, verbosity):
    break_project_index(context)
    try:
        return subprocess.run(command(context, "--function", "root", "--recover-missing",
                                      verbosity=verbosity), capture_output=True,
                              text=True, env=context.recovery_env)
    finally:
        restore_project_index(context)
