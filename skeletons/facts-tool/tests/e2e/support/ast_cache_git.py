"""Real committed C++ fixture inputs, excluding generated caches and databases."""
import subprocess


def git(root, environment, *arguments):
    result = subprocess.run(
        ["git", "-c", "user.name=Facts Cache BDD", "-c", "user.email=facts-cache@example.invalid",
         "-c", "core.hooksPath=/dev/null", "-c", "commit.gpgSign=false",
         "-C", str(root), *arguments],
        env=environment, text=True, capture_output=True, check=False, timeout=30)
    assert result.returncode == 0, result.stdout + result.stderr
    return result.stdout.strip()


def initialize_repository(root, environment):
    git(root, environment, "init", "--quiet")
    commit_inputs(root, environment)


def commit_inputs(root, environment, *, allow_empty=False):
    paths = sorted(str(path.relative_to(root)) for path in root.rglob("*")
                   if path.is_file() and path.suffix in (".cpp", ".hpp")
                   and ".git" not in path.relative_to(root).parts)
    assert paths, "Git cache fixture has no C++ inputs to commit"
    git(root, environment, "add", "--", *paths)
    changed = git(root, environment, "diff", "--cached", "--name-only")
    if changed or allow_empty:
        git(root, environment, "commit", "--quiet", "--allow-empty", "-m", "Commit cache fixture inputs")
    return git(root, environment, "rev-parse", "HEAD")
