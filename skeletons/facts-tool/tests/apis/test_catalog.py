"""Keep API command discovery aligned with the actual CLI parser."""
import re
import subprocess


def cli_help(executable, server, path):
    result = subprocess.run([str(executable), *path, "--help"], cwd=server.root,
                            env=server.env, capture_output=True, text=True, timeout=10)
    assert result.returncode == 0, result.stderr
    return result.stdout


def child_commands(help_text):
    sections = re.split("subcommands:", help_text, flags=re.IGNORECASE)
    if len(sections) < 2:
        return []
    section = sections[1]
    return re.findall(r"^  ([a-z][a-z0-9-]*)(?:,\s*[a-z][a-z0-9-]*)*\s{2,}",
                      section, re.MULTILINE)


def test_catalog_covers_real_cli_and_every_endpoint_accepts_help(server, executable):
    status, body = server.api.request("GET", "/v1/commands")
    assert status == 200
    catalog = {item["path"]: item["endpoint"] for item in body["commands"]}
    assert "analyse/variable-flow" in catalog
    assert "symbol/index/clear" in catalog
    assert "serve" not in catalog
    pending = [([], cli_help(executable, server, []))]
    discovered = set()
    while pending:
        prefix, help_text = pending.pop()
        for child in child_commands(help_text):
            if child == "serve":
                continue
            path = [*prefix, child]
            discovered.add("/".join(path))
            pending.append((path, cli_help(executable, server, path)))
    assert discovered
    assert discovered.issubset(catalog), discovered - catalog.keys()
    for path, endpoint in catalog.items():
        assert endpoint == f"/v1/commands/{path}"
        result = server.api.run(["--help"], path)
        assert "facts-tool" in result["stdout"], (path, result)
        assert "--help" in result["stdout"], (path, result)
