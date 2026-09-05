from __future__ import annotations

import os
import subprocess
from pathlib import Path

from pytest_bdd import given, then, when


@given("a YAML defaults file with one compiler token")
def defaults_file(context, tmp_path: Path) -> None:
    context.defaults_root = tmp_path
    context.defaults_file = tmp_path / "defaults.yaml"
    context.defaults_file.write_text(
        "conf_root: " + str(tmp_path / "store") + "\n"
        "conf_template: '{filename}.db'\n"
        "extra_args: ['-std=c++23']\n",
        encoding="utf-8",
    )


@when("I show configuration defaults from that directory")
def show_defaults(context) -> None:
    result = subprocess.run(
        [str(context.facts_tool), "config", "show", "--config", "defaults.yaml"],
        cwd=context.defaults_root,
        env=os.environ.copy(),
        capture_output=True,
        text=True,
        check=False,
    )
    context.last_returncode = result.returncode
    context.last_output = result.stdout + result.stderr


@then("configuration output names the YAML source and compiler token")
def output_has_defaults(context) -> None:
    assert context.last_returncode == 0, context.last_output
    assert "defaults.yaml" in context.last_output
    assert "[-std=c++23]" in context.last_output


@then("configuration inspection creates no generated database")
def no_generated_database(context) -> None:
    assert not (context.defaults_root / "store").exists()
