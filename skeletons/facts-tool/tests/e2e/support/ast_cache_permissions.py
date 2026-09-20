"""Run CLI permission checks with filesystem write restrictions enforced."""
from contextlib import contextmanager
import os
from pathlib import Path
import pwd
import shutil
import subprocess
from tempfile import TemporaryDirectory

from support.ast_cache import AstCacheProject
from support.ast_cache_assertions import require_stored
from support.ast_cache_metadata import require_imported_artifact


@contextmanager
def read_only_project(context, *, cache_enabled):
    # A direct temporary directory stays accessible when a root-run test drops
    # privileges; pytest's own temporary-directory ancestors can be mode 0700.
    with TemporaryDirectory(prefix="facts-cache-readonly-") as directory:
        project = AstCacheProject.create(context, Path(directory))
        project.configure(ast_cache=cache_enabled)
        if cache_enabled:
            project.run("import")
            require_stored(project)
            require_imported_artifact(project)
        project.conf.chmod(0o444)
        project.project_bytes_before = project.conf.read_bytes()
        project.permission_options = {}
        # Root in a restricted user namespace may already lack permission to
        # override mode 0444. Only change identity when write access remains.
        if os.access(project.conf, os.W_OK):
            assert os.geteuid() == 0, "mode 0444 did not restrict project writes"
            account = pwd.getpwnam("nobody")
            for path in (project.root, *project.root.rglob("*")):
                os.chown(path, account.pw_uid, account.pw_gid)
            project.permission_options = {
                "user": account.pw_uid, "group": account.pw_gid, "extra_groups": [],
            }
            executable = subprocess.run(
                ["test", "-x", str(project.tool)], check=False,
                **project.permission_options,
            )
            if executable.returncode != 0:
                copied_tool = project.root / "facts-tool"
                shutil.copy2(project.tool, copied_tool)
                copied_tool.chmod(0o755)
                project.tool = copied_tool
        try:
            yield project
        finally:
            project.conf.chmod(0o644)


def run_without_project_writes(project, family, *, command=None):
    access = subprocess.run(
        ["test", "-w", str(project.conf)], check=False,
        **project.permission_options,
    )
    assert access.returncode == 1, "the test process can still write the project database"
    project.last_family = family
    project.last = subprocess.run(
        command or project.command(family), cwd=project.root, env=project.environment,
        text=True, capture_output=True, timeout=60, check=False,
        **project.permission_options,
    )
